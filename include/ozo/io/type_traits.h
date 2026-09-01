#pragma once

#include <ozo/io/istream.h>
#include <ozo/io/ostream.h>

namespace ozo {

template <typename T>
concept Resizable = requires (T v) { v.resize(std::size_t()); };


template <typename T>
concept Writable = requires (istream& in, T v) { read(in, v); };


template <typename T>
concept Readable = requires (ostream& out, T v) { write(out, v); };


} // namespace ozo
