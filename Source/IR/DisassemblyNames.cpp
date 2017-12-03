#include "Inline/BasicTypes.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "IR.h"
#include "Module.h"

using namespace Serialization;

enum class NameSubsectionType : U8
{
	module = 0,
	function = 1,
	local = 2,
	label = 3,
	type = 4,
	table = 5,
	memory = 6,
	global = 7,
	invalid = 0xff
};

namespace IR
{
	void deserializeNameMap(InputStream& stream,std::vector<std::string>& outNames)
	{
		Uptr numNames = 0;
		serializeVarUInt32(stream,numNames);
		for(Uptr serializedNameIndex = 0;serializedNameIndex < numNames;++serializedNameIndex)
		{
			Uptr nameIndex = 0;
			serializeVarUInt32(stream,nameIndex);

			std::string nameString;
			serialize(stream,nameString);

			if(nameIndex >= outNames.size()) { outNames.resize(nameIndex + 1); }
			outNames[nameIndex] = std::move(nameString);
		}
	}

	void serializeNameMap(OutputStream& stream,const std::vector<std::string>& outNames)
	{
		Uptr numNames = 0;
		for(Uptr nameIndex = 0;nameIndex < outNames.size();++nameIndex)
		{
			if(outNames[nameIndex].size()) { ++numNames; }
		}

		serializeVarUInt32(stream,numNames);
		for(Uptr nameIndex = 0;nameIndex < outNames.size();++nameIndex)
		{
			if(outNames[nameIndex].size())
			{
				serializeVarUInt32(stream,nameIndex);

				std::string nameString = outNames[nameIndex];
				serialize(stream,nameString);
			}
		}
	}

	void getDisassemblyNames(const Module& module,DisassemblyNames& outNames)
	{
		// Fill in the output with the correct number of blank names.
		for(const auto& functionImport : module.functions.imports)
		{
			DisassemblyNames::Function functionNames;
			functionNames.locals.resize(module.types[functionImport.type.index]->parameters.size());
			outNames.functions.push_back(std::move(functionNames));
		}
		for(Uptr functionDefIndex = 0;functionDefIndex < module.functions.defs.size();++functionDefIndex)
		{
			const FunctionDef& functionDef = module.functions.defs[functionDefIndex];
			DisassemblyNames::Function functionNames;
			functionNames.locals.insert(functionNames.locals.begin(),module.types[functionDef.type.index]->parameters.size() + functionDef.nonParameterLocalTypes.size(),"");
			outNames.functions.push_back(std::move(functionNames));
		}
		
		outNames.types.insert(outNames.types.end(),module.types.size(),"");
		outNames.tables.insert(outNames.tables.end(),module.tables.defs.size() + module.tables.imports.size(),"");
		outNames.memories.insert(outNames.memories.end(),module.memories.defs.size() + module.memories.imports.size(),"");
		outNames.globals.insert(outNames.globals.end(),module.globals.defs.size() + module.globals.imports.size(),"");

		// Deserialize the name section, if it is present.
		Uptr userSectionIndex = 0;
		if(findUserSection(module,"name",userSectionIndex))
		{
			try
			{
				const UserSection& nameSection = module.userSections[userSectionIndex];
				MemoryInputStream stream(nameSection.data.data(),nameSection.data.size());
			
				while(stream.capacity())
				{
					U8 subsectionType = (U8)NameSubsectionType::invalid;
					serializeVarUInt7(stream,subsectionType);

					U32 numSubsectionBytes = 0;
					serializeVarUInt32(stream,numSubsectionBytes);
					
					MemoryInputStream substream(stream.advance(numSubsectionBytes),numSubsectionBytes);
					switch((NameSubsectionType)subsectionType)
					{
					case NameSubsectionType::module:
					{
						serialize(substream,outNames.moduleName);
						break;
					}
					case NameSubsectionType::function:
					{
						U32 numFunctionNames = 0;
						serializeVarUInt32(substream,numFunctionNames);
						for(Uptr functionNameIndex = 0;functionNameIndex < numFunctionNames;++functionNameIndex)
						{
							U32 functionIndex = 0;
							serializeVarUInt32(substream,functionIndex);

							std::string functionName;
							serialize(substream,functionName);

							if(functionIndex < outNames.functions.size()) { outNames.functions[functionIndex].name = std::move(functionName); }
						}
						break;
					}
					case NameSubsectionType::local:
					{
						U32 numFunctionLocalNameMaps = 0;
						serializeVarUInt32(substream,numFunctionLocalNameMaps);
						for(Uptr functionNameIndex = 0;functionNameIndex < numFunctionLocalNameMaps;++functionNameIndex)
						{
							U32 functionIndex = 0;
							serializeVarUInt32(substream,functionIndex);

							if(functionIndex < outNames.functions.size())
							{
								deserializeNameMap(substream,outNames.functions[functionIndex].locals);
							}
							else
							{
								Log::printf(Log::Category::error,"Invalid WASM binary local name section function index: %u >= %u\n",
									Uptr(functionIndex),outNames.functions.size()
									);
								break;
							}
						}

						break;
					}
					case NameSubsectionType::label:
					{
						U32 numFunctionLabelNameMaps = 0;
						serializeVarUInt32(substream,numFunctionLabelNameMaps);
						for(Uptr functionNameIndex = 0;functionNameIndex < numFunctionLabelNameMaps;++functionNameIndex)
						{
							U32 functionIndex = 0;
							serializeVarUInt32(substream,functionIndex);

							if(functionIndex < outNames.functions.size())
							{
								deserializeNameMap(substream,outNames.functions[functionIndex].labels);
							}
							else
							{
								Log::printf(Log::Category::error,"Invalid WASM binary label name section function index: %u >= %u\n",
									Uptr(functionIndex),outNames.functions.size()
									);
								break;
							}
						}

						break;
					}
					case NameSubsectionType::type: deserializeNameMap(substream,outNames.types); break;
					case NameSubsectionType::table: deserializeNameMap(substream,outNames.tables); break;
					case NameSubsectionType::memory: deserializeNameMap(substream,outNames.memories); break;
					case NameSubsectionType::global: deserializeNameMap(substream,outNames.globals); break;
					default:
						Log::printf(Log::Category::error,"Unknown WASM binary name subsection type: %u\n",Uptr(subsectionType));
						break;
					};
				};
			}
			catch(FatalSerializationException exception)
			{
				Log::printf(Log::Category::debug,"FatalSerializationException while deserializing WASM user name section: %s\n",exception.message.c_str());
			}
		}
	}

	template<typename SerializeBody>
	void serializeNameSubsection(OutputStream& stream,NameSubsectionType type,SerializeBody serializeBody)
	{
		ArrayOutputStream subsectionStream;
		serializeBody(subsectionStream);
		serialize(stream,*(U8*)&type);
		std::vector<U8> bytes = subsectionStream.getBytes();
		serialize(stream,bytes);
	}

	void setDisassemblyNames(Module& module,const DisassemblyNames& names)
	{
		// Replace an existing name section if one is present, or create a new section.
		Uptr userSectionIndex = 0;
		if(!findUserSection(module,"name",userSectionIndex))
		{
			userSectionIndex = module.userSections.size();
			module.userSections.push_back({"name",{}});
		}

		ArrayOutputStream stream;

		// Module name
		serializeNameSubsection(stream,NameSubsectionType::module,[names](OutputStream& subsectionStream)
		{
			std::string moduleName = names.moduleName;
			serialize(subsectionStream,moduleName);
		});

		// Function names
		serializeNameSubsection(stream,NameSubsectionType::function,[names](OutputStream& subsectionStream)
		{
			Uptr numFunctionNames = names.functions.size();
			serializeVarUInt32(subsectionStream,numFunctionNames);
			for(Uptr functionIndex = 0;functionIndex < names.functions.size();++functionIndex)
			{
				serializeVarUInt32(subsectionStream,functionIndex);
				std::string functionName = names.functions[functionIndex].name;
				serialize(subsectionStream,functionName);
			}
		});

		// Local names.
		serializeNameSubsection(stream,NameSubsectionType::local,[names](OutputStream& subsectionStream)
		{
			Uptr numFunctionNames = names.functions.size();
			serializeVarUInt32(subsectionStream,numFunctionNames);
			for(Uptr functionIndex = 0;functionIndex < names.functions.size();++functionIndex)
			{
				serializeVarUInt32(subsectionStream,functionIndex);
				serializeNameMap(subsectionStream,names.functions[functionIndex].locals);
			}
		});
		
		// Label names.
		serializeNameSubsection(stream,NameSubsectionType::label,[names](OutputStream& subsectionStream)
		{
			Uptr numFunctionNames = names.functions.size();
			serializeVarUInt32(subsectionStream,numFunctionNames);
			for(Uptr functionIndex = 0;functionIndex < names.functions.size();++functionIndex)
			{
				serializeVarUInt32(subsectionStream,functionIndex);
				serializeNameMap(subsectionStream,names.functions[functionIndex].labels);
			}
		});
		
		// Type names
		serializeNameSubsection(stream,NameSubsectionType::type,[names](OutputStream& subsectionStream)
		{
			serializeNameMap(subsectionStream,names.types);
		});

		serializeNameSubsection(stream,NameSubsectionType::table,[names](OutputStream& subsectionStream)
		{
			serializeNameMap(subsectionStream,names.tables);
		});
		
		serializeNameSubsection(stream,NameSubsectionType::memory,[names](OutputStream& subsectionStream)
		{
			serializeNameMap(subsectionStream,names.memories);
		});
		
		serializeNameSubsection(stream,NameSubsectionType::global,[names](OutputStream& subsectionStream)
		{
			serializeNameMap(subsectionStream,names.globals);
		});

		module.userSections[userSectionIndex].data = stream.getBytes();
	}
}