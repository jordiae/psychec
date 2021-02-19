// Copyright (c) 2020/21 Leandro T. C. Melo <ltcmelo@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "Location.h"

using namespace psy;

Location::Location(std::string filePath, LinePositionSpan lineSpan)
    : fileLineSpan_(filePath, lineSpan)
{}

Location::Location(FileLinePositionSpan fileLineSpan)
    : fileLineSpan_(std::move(fileLineSpan))
{}

Location Location::create(std::string filePath, LinePositionSpan span)
{
    Location loc(filePath, span);
    return loc;
}

Location Location::create(FileLinePositionSpan fileLineSpan)
{
    return Location(std::move(fileLineSpan));
}

namespace psy {

bool operator==(const Location& a, const Location& b)
{
    return a.lineSpan() == b.lineSpan();
}

std::ostream& operator<<(std::ostream& os, const Location& loc)
{
    os << loc.lineSpan();
    return os;
}

} // psy
