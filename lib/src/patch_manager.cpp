#include "rebear/patch_manager.h"
#include "rebear/spi_protocol_network.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace rebear {

bool PatchManager::addPatch(const Patch& patch) {
    if (!patch.isValid()) {
        setError("Invalid patch: validation failed");
        return false;
    }

    patches_[patch.id] = patch;
    return true;
}

bool PatchManager::removePatch(uint8_t id) {
    auto it = patches_.find(id);
    if (it == patches_.end()) {
        setError("Patch ID " + std::to_string(id) + " not found");
        return false;
    }

    patches_.erase(it);
    return true;
}

const Patch* PatchManager::getPatch(uint8_t id) const {
    auto it = patches_.find(id);
    if (it == patches_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<Patch> PatchManager::getPatches() const {
    std::vector<Patch> result;
    result.reserve(patches_.size());
    
    for (const auto& pair : patches_) {
        result.push_back(pair.second);
    }
    
    return result;
}

void PatchManager::clearLocal() {
    patches_.clear();
}

bool PatchManager::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        setError("Failed to open file for writing: " + filename);
        return false;
    }

    // Write JSON manually (simple format, no external dependencies)
    file << "{\n";
    file << "  \"patches\": [\n";

    bool first = true;
    for (const auto& pair : patches_) {
        const Patch& patch = pair.second;

        if (!first) {
            file << ",\n";
        }
        first = false;

        file << "    {\n";
        file << "      \"id\": " << static_cast<int>(patch.id) << ",\n";
        file << "      \"address\": \"0x" << std::hex << std::setw(6) 
             << std::setfill('0') << patch.address << "\",\n";
        file << std::dec; // Reset to decimal
        
        // Write data as hex string
        file << "      \"data\": \"";
        for (size_t i = 0; i < patch.data.size(); ++i) {
            file << std::hex << std::setw(2) << std::setfill('0') 
                 << static_cast<int>(patch.data[i]);
        }
        file << "\",\n";
        file << std::dec; // Reset to decimal
        
        file << "      \"enabled\": " << (patch.enabled ? "true" : "false") << "\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";

    if (!file.good()) {
        setError("Error writing to file: " + filename);
        return false;
    }

    return true;
}

bool PatchManager::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        setError("Failed to open file for reading: " + filename);
        return false;
    }

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    if (content.empty()) {
        setError("File is empty: " + filename);
        return false;
    }

    // Simple JSON parser for our specific format
    // This is a minimal parser - not a full JSON implementation
    std::map<uint8_t, Patch> newPatches;

    try {
        size_t pos = 0;
        
        // Find "patches" array
        pos = content.find("\"patches\"", pos);
        if (pos == std::string::npos) {
            setError("Invalid JSON: 'patches' array not found");
            return false;
        }

        pos = content.find('[', pos);
        if (pos == std::string::npos) {
            setError("Invalid JSON: array start not found");
            return false;
        }
        pos++;

        // Parse each patch object
        while (true) {
            // Find next object start
            pos = content.find('{', pos);
            if (pos == std::string::npos) {
                break; // No more patches
            }
            pos++;

            Patch patch;

            // Parse id
            size_t idPos = content.find("\"id\"", pos);
            if (idPos == std::string::npos || idPos > content.find('}', pos)) {
                break;
            }
            idPos = content.find(':', idPos) + 1;
            patch.id = static_cast<uint8_t>(std::stoi(content.substr(idPos)));

            // Parse address
            size_t addrPos = content.find("\"address\"", pos);
            if (addrPos == std::string::npos || addrPos > content.find('}', pos)) {
                setError("Invalid JSON: address not found");
                return false;
            }
            addrPos = content.find("\"0x", addrPos) + 1;
            size_t addrEnd = content.find('\"', addrPos);
            std::string addrStr = content.substr(addrPos, addrEnd - addrPos);
            patch.address = std::stoul(addrStr, nullptr, 16);

            // Parse data
            size_t dataPos = content.find("\"data\"", pos);
            if (dataPos == std::string::npos || dataPos > content.find('}', pos)) {
                setError("Invalid JSON: data not found");
                return false;
            }
            dataPos = content.find('\"', dataPos + 6) + 1;
            size_t dataEnd = content.find('\"', dataPos);
            std::string dataStr = content.substr(dataPos, dataEnd - dataPos);
            
            if (dataStr.length() != 16) {
                setError("Invalid JSON: data must be 16 hex characters (8 bytes)");
                return false;
            }

            for (size_t i = 0; i < 8; ++i) {
                std::string byteStr = dataStr.substr(i * 2, 2);
                patch.data[i] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            }

            // Parse enabled
            size_t enabledPos = content.find("\"enabled\"", pos);
            if (enabledPos == std::string::npos || enabledPos > content.find('}', pos)) {
                setError("Invalid JSON: enabled not found");
                return false;
            }
            enabledPos = content.find(':', enabledPos) + 1;
            // Skip whitespace
            while (enabledPos < content.length() && 
                   (content[enabledPos] == ' ' || content[enabledPos] == '\t' || 
                    content[enabledPos] == '\n' || content[enabledPos] == '\r')) {
                enabledPos++;
            }
            patch.enabled = (content.substr(enabledPos, 4) == "true");

            // Validate and add patch
            if (!patch.isValid()) {
                setError("Invalid patch in file: ID " + std::to_string(patch.id));
                return false;
            }

            newPatches[patch.id] = patch;

            // Move past this object
            pos = content.find('}', pos) + 1;
        }

    } catch (const std::exception& e) {
        setError(std::string("JSON parsing error: ") + e.what());
        return false;
    }

    // Success - replace current patches
    patches_ = std::move(newPatches);
    return true;
}

bool PatchManager::applyAllBuffer(SPIProtocolNetwork& spi) {
    std::vector<Patch> patchList;
    patchList.reserve(patches_.size());
    
    for (const auto& pair : patches_) {
        patchList.push_back(pair.second);
    }
    
    if (patchList.empty()) {
        return true; // Nothing to apply
    }
    
    if (!spi.uploadPatchBuffer(patchList)) {
        setError("Failed to upload patch buffer: " + spi.getLastError());
        return false;
    }
    
    return true;
}

} // namespace rebear
