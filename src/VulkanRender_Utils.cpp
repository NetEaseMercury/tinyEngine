/**
 * @file VulkanRender_Utils.cpp
 * @brief Binary file read helper for SPIR-V modules.
 */
#include "VulkanRender.hpp"
#include <fstream>
#include <vector>

/** @brief Read entire file as char vector (binary); empty if open fails */
std::vector<char> VulkanRender::readFile(const std::string& fileName)
{
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return std::vector<char>();
	}

	auto fileSize = file.tellg();
	std::vector<char> buffer(static_cast<size_t>(fileSize));

	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

	file.close();

	return buffer;
}
