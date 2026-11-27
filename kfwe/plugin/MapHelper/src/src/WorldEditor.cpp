#include "stdafx.h"
#include "WorldEditor.h"
#include "TriggerEditor.h"

#include "mapHelper.h"
#include <include\Export.h>
#include <YDPluginManager.h>
#include <HashTable.hpp>
#include <base/util/BinaryReader.h>
#include <algorithm> 
#include <unordered_set>

extern MakeEditorData* g_make_editor_data;

std::map<std::string, std::string> g_config_map;

#define RELEASE(ptr) if (ptr) { delete ptr; ptr = nullptr;}

WorldEditor::WorldEditor()
{
	m_currentData = nullptr;
	m_tempPath = nullptr;
}

WorldEditor::~WorldEditor()
= default;

//WorldEditor* WorldEditor::getInstance()
//{
//	static WorldEditor instance;
//
//	return &instance;
//}

uintptr_t WorldEditor::getAddress(uintptr_t addr)
{
	const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	return addr - 0x00400000 + base;
}


void WorldEditor::loadConfigData()
{
	auto& triggerEditor = get_trigger_editor();

	TriggerConfigData* configData = nullptr;

	if (g_make_editor_data)
	{
		configData = g_make_editor_data->config_data;
	}
	else
	{
		configData = std_call<TriggerConfigData*>(getAddress(0x004D4DA0));
	}

	triggerEditor.loadTriggerConfig(configData);
}

EditorData* WorldEditor::getEditorData()
{
	if (g_make_editor_data)
	{
		return g_make_editor_data->editor_data;
	}

	if (m_currentData) //If data is currently available, then use the current data.
	{
		return m_currentData;
	}

	uintptr_t addr = *(uintptr_t*)getAddress(0x803cb0);
	uintptr_t index = *(uintptr_t*)(addr + 0x1b0); //Current map index

	uintptr_t object = *(uintptr_t*)(*(uintptr_t*)(addr + 0x1a8) + index * 4);

	if (*(uintptr_t*)(object + 0x114))
	{
		uintptr_t uknow = *(uintptr_t*)(object + 0x118);

		return *(EditorData**)uknow;
	}
	MessageBoxA(0, "Unable to obtain editor object", "Error!!!", MB_OK);
	return 0;
}

void WorldEditor::saveMap(const char* outPath)
{
	this_call<int>(getAddress(0x0055ccd0), getEditorData(), outPath);
}



const char* WorldEditor::getCurrentMapPath()
{
	auto data = getEditorData();

	return data->mappath;
}

const char* WorldEditor::getTempSavePath()
{
	return m_tempPath;
}

int WorldEditor::getSoundDuration(const char* path)
{
	if (g_make_editor_data)
	{
		return g_make_editor_data->get_sound_duration(path);
	}
	uint32_t param[10];
	ZeroMemory(&param, sizeof param);
	auto& v_we = get_world_editor();
	fast_call<int>(v_we.getAddress(0x004DCFA0), path, &param);
	return param[1];
}


std::string WorldEditor::getConfigData(const std::string& parentKey, const std::string& childKey, int index)
{
	if (g_make_editor_data)
	{
		std::string key = parentKey + childKey + std::to_string(index);
		auto it = g_config_map.find(key);
		if (it == g_config_map.end())
		{
			std::string ret = g_make_editor_data->get_config_data(parentKey.c_str(), childKey.c_str(), index);

			g_config_map[key] = ret;
			return ret;
		}
		return it->second;
	}

	auto config = mh::get_config_table();

	const char* value = config->get_value(parentKey.c_str(), childKey.c_str(), index);
	
	if (!value)
	{
		return std::string();
	}

	auto real_value = config->get_value("WorldEditStrings", value, 0);
	if (real_value) {
		return std::string(real_value);
	}
	return std::string(value);
}


bool WorldEditor::getSkillObjectData(uint32_t id,uint32_t level,std::string text, std::string& value)
{
	if (g_make_editor_data)
	{
		value = g_make_editor_data->get_skill_data(id, level, text.c_str());
		return true;
	}
	uint32_t data = std_call<uint32_t>(getAddress(0x004D4EE0));
	char buffer[0x400];
	bool ret = this_call<bool>(getAddress(0x0050B7B0), data, id, text.c_str(), buffer, 0x400, level, 1);
	if (ret) value = buffer;
	return ret;
}


bool WorldEditor::hasSkillByUnit(uint32_t unit_id, uint32_t skill_id)
{
	if (g_make_editor_data) {
		return g_make_editor_data->has_skill_by_unit(unit_id, skill_id);
	}

	uintptr_t data = c_call<uintptr_t>(getAddress(0x004D4CA0));
	return this_call<bool>(getAddress(0x00501020), data, unit_id, skill_id, 0);
}

void WorldEditor::onSaveMap(const char* tempPath, EditorData* data, int isExpansion, const char* destPath)
{
	m_tempPath = tempPath;
	m_currentData = data;
	m_isExpansion = isExpansion;
	m_destPath = destPath;

	print("��ǰ��ͼ·��%s\n", getCurrentMapPath());
	print("�����ͼ·�� %s\n", getTempSavePath());


	auto& triggerEditor = get_trigger_editor();

	TriggerData* triggerData = getEditorData()->triggers;

	triggerEditor.loadTriggers(triggerData);


#if defined(EMBED_YDWE)
	int ret = 6;
#else
	int ret = 0;
	auto& v_helper = get_helper();

	if (v_helper.getConfig() & Helper::CONFIG::SUPPER_SPEED_SAVE) {
		ret = 6;
	}
	//if (result == -1)
	//{
	//
	//	v_helper.setMenuEnable(false);
	//
	//	ret = MessageBoxA(0, "Should we save using the new save mode?", "Seven's Accelerator", MB_SYSTEMMODAL | MB_YESNO);
	//
	//	v_helper.setMenuEnable(true);
	//
	//	if (ret == 6)
	//		print("Custom save mode\n");
	//	else
	//		print("Original save mode\n");
	//}
	//else if (result == 1)
	//{
	//	ret = 6;
	//}
#endif

	clock_t start = clock();
		

	auto& manager = get_ydplugin_manager();



	if (ret == 6 && m_isExpansion)
	{

		manager.m_enable = true;


		//customSaveWts(getTempSavePath());//There's a bug, so I won't use it for now.
		saveWts();

		saveW3i();
		saveImp();
		saveW3e();
		saveShd();
		saveWpm();
		saveMiniMap();
		saveMmp();
		saveObject();

		

		saveUnits();
		saveRect();
		saveCamara();
		saveSound();

		triggerEditor.saveTriggers(getTempSavePath());
		triggerEditor.saveScriptTriggers(getTempSavePath());
		triggerEditor.saveSctipt(getTempSavePath());

		customSaveDoodas(getTempSavePath());

		//Update label
		updateSaveFlags();
	}
	else
	{
		manager.m_enable = false;

		saveWts();

		saveW3i();
		saveImp();
		saveW3e();
		saveShd();
		saveWpm();
		saveMiniMap();
		saveMmp();
		saveObject();

		saveDoodas();

		saveUnits();
		saveRect();
		saveCamara();
		saveSound();

		saveTrigger();
		saveScript();
	}


	fs::path map_path = getCurrentMapPath();

	//ֻIncremental updates will only be initiated for incremental saves and maps that are not in LNI format.
	if (v_helper.getConfig() & Helper::INCRE_RESOURCE  && map_path.filename() != ".w3x")
	{
		customSaveArchive();
	} 
	else 
	{
		saveArchive();
	}
	

	print("All map data saved. Total time: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	m_tempPath = nullptr;
	m_currentData = nullptr;
	m_isExpansion = 1;
	m_destPath = nullptr;
}

int WorldEditor::saveWts()
{

	print("Save wts text data\n");

	clock_t start = clock() ;

	int ret = this_call<int>(getAddress(0x0055DAF0), getEditorData(), getTempSavePath());

	print("wts save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveW3i()
{
	print("Save w3i map information data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0055D280), getEditorData(), getTempSavePath(), m_isExpansion);

	print("w3i saving complete. Time taken: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}
int WorldEditor::saveImp()
{
	print("Save the list of imp files\n");

	clock_t start = clock();

	uintptr_t object = *(uintptr_t*)((uintptr_t)getEditorData() + 0x3904);
	this_call<int>(getAddress(0x0051CEB0), object, getTempSavePath());
	int ret = this_call<int>(getAddress(0x0055DFD0), getEditorData(), getTempSavePath());

	//uint32_t size = *(uint32_t*)(object + 0x20c);
	//uint32_t ptr = *(uint32_t*)(object + 0x210);
	//
	//printf("size %i  ptr %x\n", size, ptr);
	//
	//for (int i = 0; i < size; i++) {
	//	uintptr_t p = ptr + i * 0x209;
	//	const char* name = (const char*)(p + 1);
	//	const char* archive_path = (const char*)(p + 0x105);
	//
	//	uint8_t byte = *(uint8_t*)(p);
	//	
	//	printf("%i %x <%s> <%s>\n", i, byte, name, archive_path);
	//
	//}
	
	//HWND hwnd = g_editor_windows[6];
	//if (hwnd) {
	//	printf("hwnd %p\n", hwnd);
	//	HANDLE handle = GetPropA(hwnd, "OsGuiPointer");
	//	if (handle) {
	//		uintptr_t ptr = *(uintptr_t*)((uintptr_t)handle + 0x10);
	//		printf("ptr1  %p\n", ptr);
	//		if (ptr) {
	//			ptr = *(uintptr_t*)(ptr + 0x8c);
	//			printf("ptr2  %p\n", ptr);
	//			if (ptr) {
	//				uint32_t size = *(uint32_t*)(ptr + 0x48);
	//				ptr = *(uintptr_t*)(ptr + 0x4c);
	//
	//				printf("size %i  ptr3 %p\n", size, ptr);
	//
	//				if (ptr && size > 0) {
	//					for (int i = 0; i < size; i++) {
	//						const char* name = (const char*)(ptr + 0x190 * i + 0x8);
	//						const char* path = (const char*)(ptr + 0x190 * i + 0x88);
	//						printf("%i {%s} {%s}\n", i, name, path);
	//					}
	//				}
	//			}
	//		}
	//		printf("handle %p\n", handle);
	//
	//	}
	//}
	print("imp save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveW3e()
{
	print("Save w3e terrain texture data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x005B0C50), getEditorData()->terrain, getTempSavePath());

	print("w3e saving complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;

}

int WorldEditor::saveShd()
{
	print("Save shd terrain shadow data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0055d1f0), getEditorData(), getTempSavePath());

	print("shd save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}


int WorldEditor::saveWpm()
{
	print("Save wpm terrain path data\n");

	clock_t start = clock();


	std::string path = std::string(getTempSavePath()) + ".wpm";
	int ret = this_call<int>(getAddress(0x005E91C0), getEditorData()->terrain, path.c_str());

	print("wpm save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveMiniMap()
{
	print("Save minimap data\n");

	clock_t start = clock();

	std::string path = std::string(getTempSavePath()) + "Map.tga";

	int ret = this_call<int>(getAddress(0x00583200), getEditorData(), path.c_str(), 0);

	print("Minimap saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}


int WorldEditor::saveMmp()
{
	print("Data for saving the mmp preview file\n");

	clock_t start = clock();
	int ret = this_call<int>(getAddress(0x00583D00), getEditorData(), getTempSavePath());

	print("mmp Saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveObject()
{
	print("Save objects\n");

	clock_t start = clock();

	std::string path = std::string(getTempSavePath()) + "Map.tga";


	uintptr_t object = *(uintptr_t*)((uintptr_t)getEditorData() + 0x3908);
	this_call<int>(getAddress(0x00518CA0), object, getTempSavePath());

	object = *(uintptr_t*)((uintptr_t)getEditorData() + 0x390c);
	this_call<int>(getAddress(0x00518CA0), object, getTempSavePath());

	int ret = this_call<int>(getAddress(0x0051B5B0), getEditorData()->objects, getTempSavePath(), m_isExpansion);

	print("Objects editing and saving completed. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;;
}

int WorldEditor::saveDoodas()
{
	print("Save war3map.doo terrain decorations\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0062BAE0), getEditorData()->doodas, getTempSavePath(), m_isExpansion);

	print("war3map.doo saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}


int WorldEditor::saveUnits()
{
	print("Save war3mapUnit.doo terrain unit preset data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0062BAE0), getEditorData()->units, getTempSavePath(), m_isExpansion);

	print("war3mapUnit.doo saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveRect()
{
	print("Save w3r terrain region data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0062ACF0), getEditorData()->regions, getTempSavePath());

	print("w3r saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}


int WorldEditor::saveCamara()
{
	print("Save w3c preset lens data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x005AEBB0), getEditorData()->cameras, getTempSavePath());

	print("w3c save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}
int WorldEditor::saveSound()
{
	print("Save w3s audio data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x005EACE0), getEditorData()->sounds, getTempSavePath());

	print("w3s save complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveTrigger()
{
	print("Save wtg + wct trigger data\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x00520ED0), getEditorData()->triggers, getTempSavePath(), m_isExpansion);

	print("wtg + wct saving complete. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}

int WorldEditor::saveScript()
{
	print("Save the war3map.j script file\n");

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0055DA80), getEditorData(), getTempSavePath());

	print("war3map.j saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return ret;
}



int WorldEditor::saveArchive()
{

	fs::path path = fs::path(getTempSavePath());
	path.remove_filename();

	if (path.string().substr(path.string().size() - 1) == "\\")
		path = fs::path(path.string().substr(0, path.string().size() - 1));

	std::string name = path.filename().string();

	name = name.substr(0,name.length() - 4);

	fs::path pathTemp = path / name;

	print("Packaging: Package the folder into an MPQ structure.\n");

	print("Path %s\n", path.string().c_str());

	clock_t start = clock();

	int ret = this_call<int>(getAddress(0x0055D720), getEditorData(), pathTemp.string().c_str(), m_isExpansion);
	
	if (ret)
	{
		path.remove_filename();

		fs::path path2 = (m_destPath && *m_destPath) ? fs::path(m_destPath) : (path / name);

		//�ƶ��ļ�Ŀ¼
		ret = fast_call<int>(getAddress(0x004D0F60), pathTemp.string().c_str(), path2.string().c_str(), 1, 0);

		print("Map packaging completed. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

		return ret;

	}

	
	return 0;
}


int WorldEditor::customSaveArchive() {
	fs::path path = fs::path(getTempSavePath());
	path.remove_filename();

	if (path.string().substr(path.string().size() - 1) == "\\")
		path = fs::path(path.string().substr(0, path.string().size() - 1));

	std::string name = path.filename().string();

	name = name.substr(0, name.length() - 4);

	fs::path temp_map_path = path.parent_path() / ("_temp_" + name);

	fs::path source_map_path = getCurrentMapPath();

	if (!fs::exists(source_map_path)) {
		source_map_path = path.parent_path() / name;
	}

	fs::path target_map_path = path.parent_path() / name;

	auto data = getEditorData();

	print("Incremental update of map file resources\n");

	print("Source image path %s\n", source_map_path.string().c_str());

	print("Temporary path %s\n", path.string().c_str());

	print("Target path %s\n", target_map_path.string().c_str());

	clock_t start = clock();

	//If the file does not exist in the source file path, then create the map to be saved.
	if (!fs::exists(source_map_path)) {
		mpq::MPQ newmap;
		newmap.create(source_map_path, 0x64, false);
		newmap.close();
	}

	if (!fs::exists(source_map_path) || !CopyFileA(source_map_path.string().c_str(), temp_map_path.string().c_str(), 0)) {
		return 0;
	}

	//Disable MPQ usage of the source image.
	this_call<void>(getAddress(0x005261D0), data->mappath);

	auto& ydplugin = get_ydplugin_manager();

	mpq::MPQ mpq;

	if (!mpq.open(temp_map_path, 0)) {
		printf("Failed to open map file; incremental mode failed; cancel incremental mode and save again.\n");
		return 0;
	}
	auto file_list = new std::unordered_set<fs::path>();

	auto ignore_map = new std::map<std::string, bool>();

	ignore_map->emplace(name, true);

	std::function<void()> add_temp_files = [&]() {
		//Replacement file list
		for (const auto i : fs::recursive_directory_iterator(path)) {
			if (!i.is_regular_file()) {
				continue;
			}
			std::string str = i.path().filename().string();
			std::transform(str.begin(), str.end(), str.begin(), ::tolower);

			if (ignore_map->find(str) != ignore_map->end()) {
				continue;
			}
			auto source = i.path();
			auto target = fs::relative(source, path);
			mpq.file_add(source, target);
		}
	};

	//war3mapImported
	std::function get_default_path = [](fs::path filename) -> fs::path
	{
		if(filename.parent_path() == "war3mapImported")
		{
			return filename;
		}
		return  fs::path("war3mapImported") / filename;
	};

	std::function parser_imp = [file_list,&mpq,&get_default_path]()
	{
		if (!mpq.file_exists("war3map.imp"))
			return;
		auto imp = mpq.file_open("war3map.imp");
		BinaryReader reader(imp.read());
		reader.advance(4);
		const auto count = reader.read<uint32_t>();
		for (int i = 0; i < count; ++i)
		{
			auto flag = reader.read<uint8_t>();
			fs::path path = reader.read_c_string();
			if (!(flag & 1))
				path = get_default_path(path);
			file_list->insert(path);
		}
	};



	//Statistics
	{
		uintptr_t object = *(uintptr_t*)((uintptr_t)data + 0x3904);
		uint32_t size = *(uint32_t*)(object + 0x20c);
		uint32_t ptr = *(uint32_t*)(object + 0x210);

		fs::path import_base_path = (const char*)(object);

		parser_imp();

		for (int i = 0; i < size; i++) {
			uintptr_t p = ptr + i * 0x209;
			uint8_t flag = *(uint8_t*)p;
			fs::path origin_path = (const char*)(p + 1);
			if (!(flag & 1))
				origin_path = get_default_path(origin_path);
			if (file_list->find(origin_path) != file_list->end())
				file_list->erase(origin_path);
		}

		for (const auto & file : *file_list)
		{
			mpq.file_remove(file);
			printf("File deletion <%s>\n", file.string().c_str());
		}


		//Traverse the file list
		for (int i = 0; i < size; i++) {
			uintptr_t p = ptr + i * 0x209;
			//The fourth flag: Add 0 (add) 1 (not add)
			//The third flag indicates the target file location: 0 for the default path and 1 for a custom path.
			//The second flag indicates whether to rename: 0 for no rename and 1 for rename.
			//The first flag indicates the original file location: 0 for the default path and 1 for a custom path.
			uint8_t flag = *(uint8_t*)p;
			const char* import_path = (const char*)(p + 1);
			const char* archive_path = (const char*)(p + 0x105);
			uint8_t byte = *(uint8_t*)(p);
			bool add_file = !(flag & (1 << 3));
			bool target_default = !(flag & (1 << 2));
			bool rename_file = flag & (1 << 1);
			bool origin_default = !(flag & 1);

			if (import_path && *import_path) {
				std::string str(import_path);
				std::transform(str.begin(), str.end(), str.begin(), ::tolower);
			
				//If there are imported files, ignore those generated by the editor.
				switch (hash_(str.c_str())) {
				case "war3mapmap.blp"s_hash:
				case "war3mapmap.tga"s_hash:
					ignore_map->emplace("war3mapmap.blp", true);
					ignore_map->emplace("war3mapmap.tga", true);
					break;
				default:
					break;
				}
			}
			uint8_t new_flag = 8;
			fs::path origin_path = import_path;
			if (origin_default)
				origin_path = get_default_path(origin_path);
			if(add_file)//If adding files for
			{
				fs::path target_path = origin_path;
				fs::path local_path = import_base_path / origin_path;
				mpq.file_add(local_path,target_path);
				printf("File addition <%s> = <%s>\n", local_path.string().c_str(), target_path.string().c_str());
				*(uint8_t*)p = new_flag;
				strcpy_s((char*)(p + 1), 0x104, import_path);
			}
			else
			{
				fs::path local_path = import_base_path / origin_path;
				if (fs::exists(local_path))
				{
					mpq.file_add(local_path, origin_path);
					printf("File replacement <%s> = <%s>\n", local_path.string().c_str(), origin_path.string().c_str());
				}
			}

			if(rename_file)
			{
				fs::path target_path = archive_path;
				mpq.file_rename(origin_path,target_path);
				printf("File rename <%s> = <%s>\n", origin_path.string().c_str(), target_path.string().c_str());
				if(!target_default)
				{
					new_flag |= 1;
					new_flag |= 1 << 2;
				}
				*(uint8_t*)p = new_flag;
				strcpy_s((char*)(p + 1), 0x104, target_path.string().c_str());
			}
			

		}
		fs::remove_all(import_base_path);
	}

	add_temp_files();

	auto& helper = get_helper();

	
	if (fs::exists(helper.ydwe_path / "compiler" / "script" / "init.lua")) {
		// The judgment is ydwe1.32

		fs::path newTempPath = path / name;

		//Compile the created empty map
		mpq::MPQ newmap;
		newmap.create(newTempPath, 0x64, false);
		newmap.close();

		//ִExecute the compilation process of ydwe
		ydplugin.on_save_event(base::a2u(newTempPath.string()), data->is_test);

		//Add the compiled files to the temp map.
		add_temp_files();

		mpq.compact(); 
		mpq.close();
	
	} else {
		//Otherwise it is ydwe1.31

		mpq.compact();
		mpq.close();

		//ִExecute the compilation process of ydwe
		ydplugin.on_save_event(base::a2u(temp_map_path.string()), data->is_test);
	}
	
	//Move file directory
	int ret = fast_call<int>(getAddress(0x004D0F60), temp_map_path.string().c_str(), target_map_path.string().c_str(), 1, 0);

	print("Map packaging completed. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	RELEASE(file_list);
	RELEASE(ignore_map);

	return 1;
}

int WorldEditor::customSaveWts(const char* path)
{
	print("Custom saving of war3map.wts text data\n");

	clock_t start = clock();

	BinaryWriter writer;
	auto string_data = getEditorData()->strings;


	writer.write<uint8_t>(0xEF);
	writer.write<uint8_t>(0xBB);
	writer.write<uint8_t>(0xBF);

	std::vector<TriggerString*> list(string_data->count);
	for (size_t i = 0; i < string_data->count; i++)
	{
		TriggerString* info = &string_data->array[i];
		list[i] = info;
	}
	std::sort(list.begin(), list.end(), [&](TriggerString* a, TriggerString* b) 
	{
		return a->index < b->index;
	});

	for (auto& i : list)
	{
		writer.write_string("STRING " + std::to_string(i->index));
		writer.write_string("\r\n{\r\n");
		writer.write_string(std::string(i->str));
		writer.write_string("\r\n}\r\n\r\n");
	}
	std::ofstream out(std::string(path) + ".wts", std::ios::binary);
	writer.finish(out);
	out.close();
	print("Saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return 1;
}


int WorldEditor::customSaveDoodas(const char* path)
{
	print("Custom-saved war3map.doo terrain decorations\n");

	clock_t start = clock();
	auto doodas = getEditorData()->doodas; 

	auto& editor = get_trigger_editor();

	auto& variableTable = editor.variableTable;

	BinaryWriter writer; 


	writer.write_string("W3do"); 


	uint32_t write_version = 0x8; 

	writer.write<uint32_t>(write_version); 


	uint32_t write_subversion = 0xB; 

	writer.write<uint32_t>(write_subversion); 


	writer.write<uint32_t>(doodas->unit_count); 


	uint32_t object = *(uint32_t*)(((uint32_t)doodas) + 0xE0);
	uint32_t addr = *(uint32_t*)((*(uint32_t*)(doodas)) + 0xc8);

	char buffer[0x100];

	for (size_t i = 0;i<doodas->unit_count;i++)
	{
		Unit* unit = &doodas->array[i]; 

		writer.write_string(std::string(unit->name,0x4)); 
		writer.write<uint32_t>(unit->variation); 
		writer.write<float>(unit->x); 
		writer.write<float>(unit->y); 
		writer.write<float>(unit->z); 
		writer.write<float>(unit->angle);  
		writer.write<float>(unit->scale_x); 
		writer.write<float>(unit->scale_y); 
		writer.write<float>(unit->scale_z); 

		sprintf(buffer, "gg_dest_%.04s_%04d", unit->name, unit->index);

	
		//Calculate the status of decorations
		uint8_t flag = 0;

		
		//The commented-out statements are inefficient and unnecessary judgments.
		//Determine if it is within the available map. True indicates it is within the boundary.

		//if (!this_call<int>(getAddress(0x005E73A0), object, &unit->x))
		//	flag = 1;
		
		//Check if it is referenced by a global variable
		//if (!this_call<int>(addr, doodas, i)) 

		//This should check whether dropped items have been set.
		//if (variableTable.find(buffer) == variableTable.end())
		if (unit->item_setting_count <= 0 && unit->item_table_index == -1 && variableTable.find(buffer) == variableTable.end())
		{
			flag |= 0x2;
		}
		else
		{
			flag &= 0xfd;
		}

		//Does it include flight altitude? Decorative elements whose altitude has been set using Ctrl + PageUp or PageDown in the terrain editor.
		if (*(uint8_t*)((uint32_t)unit + 0x84) & 0x8)
		{
			flag |= 0x4;
		}
		writer.write<uint8_t>(flag);
		writer.write<uint8_t>(unit->doodas_life); 
		writer.write<int32_t>(unit->item_table_index); 
		writer.write<uint32_t>(unit->item_setting_count2); 

		for (size_t a = 0; a < unit->item_setting_count2;a++)
		{
			ItemTableSetting* setting = unit->item_setting; 
			writer.write<uint32_t>(setting->info_count2); 

			for (size_t b = 0; b < setting->info_count2; b++)
			{
				ItemTableInfo* info = &setting->item_infos[b]; 
				// Empty items are such a rip-off!
				// I'm afraid other positions will need to be changed.
				// This is the direct item unit; the drop rate is set on the destructible item side.
				if (*(uint32_t*)(info->name) > 0) {
					writer.write_string(std::string(info->name, info->name + 0x4));
				}
				else
					writer.write_string("\0\0\0\0");
				writer.write<uint32_t>(info->rate); 
			}
		}

		writer.write<uint32_t>(unit->index); 
	}
	uint32_t write_special_version = 0;
	writer.write<uint32_t>(write_special_version);
	writer.write<uint32_t>(doodas->special_table->special_doodas_count); 

	for (size_t i = 0; i< doodas->special_table->special_doodas_count;i++)
	{
		SpecialDoodas* unit = &doodas->special_table->special[i];
		std::string id = std::string(unit->name, 0x4);
		// These are decorations.
		writer.write_string(id);
		writer.write<uint32_t>(unit->variation); 
		writer.write<uint32_t>(unit->x); 
		writer.write<uint32_t>(unit->y); 
	}


	std::ofstream out(std::string(path) + ".doo", std::ios::binary);
	writer.finish(out);
	out.close();

	print("war3map.doo Saved successfully. Time elapsed: %f Seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);

	return 1;
}


void WorldEditor::updateSaveFlags()
{
	auto world_data = getEditorData();
	if (world_data->is_test)
		return;
	auto trigger_data = world_data->triggers;
	
	trigger_data->updage_flag = 0;
	trigger_data->variables->updage_flag = 0;
	for (size_t i = 0; i < trigger_data->categoriy_count; i++)
	{
		Categoriy* categoriy = trigger_data->categories[i];
		uint32_t trigger_count = categoriy->trigger_count;
		for (uint32_t n = 0; n < trigger_count; n++)
		{
			Trigger* trigger = categoriy->triggers[n];
			trigger->updage_flag = 0;
		}
	}

	world_data->doodas->updage_flag = 0;
}

WorldEditor& get_world_editor()
{
	static WorldEditor instance;
	return instance;
}
