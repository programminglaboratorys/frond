#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <numeric>

#include <filesystem>
#include "json.hpp"
#include <fstream>
#include <vector>
#include <string_view>
#include <memory>
#include <unordered_set>

#include "Clover.h"

#include <iostream>


#define INTERNAL_FROND_parseString(data, member, key, fallback_code) \
	if (!data->contains(key) || !(*data)[key].is_string()) { \
		fallback_code; \
	} \
	m.member = (*data)[key].get_ref<const std::string&>();


#define INTERNAL_FROND_parseStringFallbackValue(data, member, key, fallback_value) \
	if (!data->contains(key) || !(*data)[key].is_string()) { \
		(*data)[key] = fallback_value; \
	} \
	m.member = (*data)[key].get_ref<const std::string&>();

#define INTERNAL_FROND_parseVersion(data, member, key, fallback_code) \
    if (!data->contains(key) || !(*data)[key].is_string()) { \
        fallback_code; \
    } \
    m.member = Clover::Version((*data)[key].get_ref<const std::string&>());

enum class ModStatus {
	Disabled,          // Discovered, but disabled
	Active,            // Enabled, validated, and ready to go
	MissingDependency, // Discovered, but required dependencies are missing from disk
	DependencyCycle,   // Discovered, but caught in a circular dependency loop
};


using json = nlohmann::json;


class Author {
public:
	std::string_view name;
	const json& metadata; // can be either a string or an object
	// TODO: move to static Load method that like ModManifest::Load() does, so that we can validate the data and return a failure if its invalid
	Author(const json& data) : metadata(data) {
		name = "Unknown Author";

		// case where its a string (e.g "iamme")
		if (data.is_string()) {
			name = data.get_ref<const std::string&>();
		}
		// other case where its an object (e.g., {"name": "iamme", ...})
		else if (data.is_object() && data.contains("name") && data["name"].is_string()) {
			name = data["name"].get_ref<const std::string&>();
		}
	}

	static bool valid(const json& data) {
		return data.is_string() || (data.is_object() && data.contains("name") && data["name"].is_string());
	}
};

class Dependency {
public:
	std::string_view id;
	Clover::VersionConstraint versionConstraint;
	Dependency(std::string_view id, Clover::VersionConstraint verc) : id(id), versionConstraint(verc) {};
	Dependency(std::string_view id, std::string_view verc_str) : id(id), versionConstraint(verc_str) {};
	Dependency() = default;
};


// NOTE: yeah its true, this kind of stuff is modloader specific, 
// even tho i want to make this a bit more generic/general purpose 
// but ofc it will need something to make it less specific to my modloader

enum class ParseError {
	None,
	Malformed,  // bad JSON syntax
	Invalid,    // missing required field
};

//static ParseError Load(std::ifstream& f, ModManifest& out);

class ModManifest {
protected:
	// remember that copying a ModManifest creates a shallow copy of the data
	std::shared_ptr<json> data;
public:
	std::string_view id;
	std::string_view name;
	std::string_view description;
	std::string_view icon; // relative path to the icon file (e.g., "icon.png")
	Clover::Version version;

	//
	std::vector<Dependency> dependencies;
	std::vector<Author> authors;
	std::vector<std::string_view> load_after;

	std::filesystem::path root{};

	ModManifest() = default;

	//ModManifest(std::ifstream& f) {
	//	from_json(json::parse(f, nullptr, false, false), *this);
	//}

	static ParseError Load(const json& j, ModManifest& m) {
		if (j.is_discarded()) {
			return ParseError::Malformed;
		}

		m.data = std::make_shared<json>(j);
		// 
		INTERNAL_FROND_parseString			   (m.data, id, "id", return ParseError::Invalid);
		INTERNAL_FROND_parseStringFallbackValue(m.data, name, "name", "Unnamed Mod");
		INTERNAL_FROND_parseStringFallbackValue(m.data, icon, "icon", "");
		INTERNAL_FROND_parseStringFallbackValue(m.data, description, "description", "");
		INTERNAL_FROND_parseVersion			   (m.data, version, "version", return ParseError::Invalid);

		if (m.data->contains("authors") && (*m.data)["authors"].is_array()) {
			for (auto& author_node : (*m.data)["authors"]) {
				if (!Author::valid(author_node)) continue;
				m.authors.emplace_back(author_node);
			}
		}

		if (m.data->contains("dependencies") && (*m.data)["dependencies"].is_object()) {
			for (auto& [key, value] : (*m.data)["dependencies"].items()) {
				if (!value.is_string()) continue;
				m.dependencies.emplace_back(key, value.get_ref<const std::string&>());
			}
		}

		if (m.data->contains("load_after") && (*m.data)["authors"].is_array()) {
			for (auto& id_node : (*m.data)["authors"]) {
				if (!id_node.is_string()) continue;
				m.load_after.emplace_back(id_node);
			}
		}

		return ParseError::None;
	}

	friend void from_json(const json& j, ModManifest& m) {
		ParseError err = Load(j, m);

		if (err != ParseError::None) {
			throw std::runtime_error("ModManifest parsing failed with error code: " + std::to_string(static_cast<int>(err)));
		}
	}

	bool operator==(const ModManifest& other) const noexcept {
		return id == other.id;
	}
};

namespace std {
	template <>
	struct hash<ModManifest> {
		std::size_t operator()(const ModManifest& m) const noexcept {
			return std::hash<std::string_view>{}(m.id);
		}
	};
}

int main()
{
	namespace fs = std::filesystem;
	std::printf("Hello. (Frond)\n");
	std::filesystem::path dir_path = "C:\\Users\\ACER\\source\\repos\\playground\\mods";


	if (!(fs::is_directory(dir_path))) {
		std::printf("expected a vaild dir path (got %ls)", dir_path.c_str());
		return -0xFF01;
	}
	std::unordered_set<ModManifest> ModEntries{};

	for (const auto& entry : fs::directory_iterator(dir_path)) {
		if (!fs::is_directory(entry)) {
			continue;
		}
		auto manifest = entry.path() / "manifest.json";
		if (!fs::exists(manifest)) {
			auto filename = entry.path().filename();
			std::printf("skipping %ls, manifest.json not found\n", filename.c_str());
			continue;
		}

		std::ifstream f(manifest);
		json j = json::parse(f, nullptr, false, false); // parse without exceptions
		ModManifest mod;
		ParseError err = ModManifest::Load(j, mod);


		if (err != ParseError::None) {
			std::printf("[Warning] Mod %ls has a parse error (error: %d)\n", entry.path().filename().c_str(), static_cast<int>(err));
			continue;
		}
		auto [_, inserted] = ModEntries.insert(mod);
		if (!inserted) {
			std::printf("[Warning] Mod %ls has a duplicate id (%.*s). skipped\n", entry.path().filename().c_str(), (int)mod.id.size(), mod.id.data());
			continue;
		}

		std::printf("Mod Loaded (name: %.*s, id: %.*s)\n",
			(int)mod.name.size(), mod.name.data(),
			(int)mod.id.size(), mod.id.data());
	}
	std::printf("done. loaded %zd mods\n", ModEntries.size());


	// MODS loaded. now 

	return 0;
}