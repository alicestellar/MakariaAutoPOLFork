#pragma once
#include <cstdint>
#include <cstddef>

void sha1(const uint8_t* data, size_t len, uint8_t out[20]);
