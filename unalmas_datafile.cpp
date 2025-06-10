#include "unalmas_datafile.h"
#include <functional>
#include <fstream>
#include <sstream>
#include <stack>
#include <limits>
#include <cassert>

namespace Unalmas
{

const std::string DataFile::EscapeString(const std::string &str, const char separator)
{
    std::string escaped;

    for (const auto c : str)
    {
        if (c == '\n') { escaped.append("\\n"); }
        else if (c == '\r') { escaped.append("\\r"); }
        else if (c == '"') { escaped.append("\""); }
        else if (c == separator) { escaped.append("\\" + std::string(1, separator)); }
        else escaped += c;
    }

    return escaped;
}

bool DataFile::Serialize(const DataFile& data,
                                const std::string& fileName,
                                const std::string& indent,
                                const char separator)
{
    const std::string comma = std::string(1, separator) + " ";

    std::function<void(const DataFile&, std::ofstream&, int)> Write = [&]
        (const DataFile& d, std::ofstream& f, int currentIndent)
    {
        auto indentStr = [&](int count, const std::string& indent) -> std::string
        {
            std::string is;
            for (int i = 0; i < count; ++i)
            {
                is += indent;
            }
            return is;
        };

        // Write contents
        if (d.GetValueCount() == 0 && d._children.size() == 0)
        {
            return;
        }

        const auto contentSize = d.GetValueCount();
        if (contentSize > 0)
        {
            f << indentStr(currentIndent, indent);

            for (auto i = 0; i < contentSize; ++i)
            {
                if (i > 0)
                {
                    f << comma;
                }

                f << EscapeString(d.GetString(i), separator);
            }

            f << "\n";
        }

        int childIndex = 0;
        for (const auto& childNode : d._children)
        {
            f << indentStr(currentIndent, indent) << "[" << d._childrenNames.at(childIndex++) << "]\n";
            f << indentStr(currentIndent, indent) << "{\n";
            Write(childNode, f, currentIndent + 1);
            f << indentStr(currentIndent, indent) << "}\n";
        }
    };

    // TODO - validate path
    //		-> e.g. it doesn't contain invalid chars "<>:\"|?*";
    //				it isn't over windows' MAX_PATH (260 char GHCPSZ)
    //				it doesn't contain directory traversal ("..")
    //				it is not the root directory, and the path actually exists

    std::ofstream file(fileName);
    if (file.is_open())
    {
        Write(data, file, 0);
        return true;
    }

    return false;
}

DataFile DataFile::FromStream(std::istream& stream, const char separator)
{
    DataFile df(8192);

    std::stack<std::reference_wrapper<DataFile>> stk;
    stk.push(df);

    auto trim = [](std::string_view& s)
    {
        const auto start = s.find_first_not_of(" \t\n\r\f\v");
        const auto end = s.find_last_not_of(" \t\n\r\f\v");
        s = (start == std::string_view::npos || end == std::string_view::npos) ?
                std::string_view{} : s.substr(start, end - start + 1);
    };

    std::string line;
    while (std::getline(stream, line))
    {
        if (stk.empty())
        {
            break;
        }

        std::string_view lineView = line;
        if (!lineView.empty())
        {
            trim(lineView);
            const bool isNode = lineView.front() == '[' && lineView.back() == ']';

            if (isNode)
            {
                const std::string nodeName(lineView.substr(1, lineView.size() - 2));
                auto& p = stk.top().get()[nodeName];
                stk.push(p);
            }
            else
            {

                if (line == "{") { continue; }
                if (line == "}") { stk.pop(); continue; }

                auto& currentNode = stk.top().get();
                std::istringstream asStringStream{ line };

                for (std::string value; std::getline(asStringStream, value, separator);)
                {
                    const int index = currentNode.GetValueCount();
                    trim(lineView = value);
                    currentNode.SetString(std::string(lineView), index);
                }
            }
        }
    }

    return df;
}

DataFile DataFile::FromString(const std::string& asString, const char separator)
{
    std::stringstream stream{ asString };
    return FromStream(stream, separator);
}

DataFile DataFile::Deserialize(const std::string& fileName, const char separator)
{
    std::ifstream file(fileName);
    if (file.is_open())
    {
        auto df = FromStream(file, separator);
        file.close();
        return df;
    }

    return DataFile{ };
}

DataFile::DataFile(std::size_t expectedChildrenCount)
{
    _children.reserve(expectedChildrenCount);
    _childIndexByName.reserve(expectedChildrenCount);
}

std::string DataFile::ToString(const std::string& indent, const char separator) const
{
    const std::string comma = std::string(1, separator) + " ";

    std::function<void(const DataFile&, std::ostream&, int)> Write = [&]
        (const DataFile& d, std::ostream& f, int currentIndent)
    {
        auto indentStr = [&](int count, const std::string& indent) -> std::string
        {
            std::string is;
            for (int i = 0; i < count; ++i)
            {
                is += indent;
            }
            return is;
        };

        // Write contents
        if (d.GetValueCount() == 0 && d._children.size() == 0)
        {
            return;
        }

        const auto contentSize = d.GetValueCount();
        if (contentSize > 0)
        {
            f << indentStr(currentIndent, indent);

            for (auto i = 0; i < contentSize; ++i)
            {
                if (i > 0)
                {
                    f << comma;
                }

                f << EscapeString(d.GetString(i), separator);
            }

            f << "\n";
        }

        int  childIndex = 0;
        for (const auto& childNode : d._children)
        {
            f << indentStr(currentIndent, indent) << "[" << d._childrenNames.at(childIndex++) << "]\n";
            f << indentStr(currentIndent, indent) << "{\n";
            Write(childNode, f, currentIndent + 1);
            f << indentStr(currentIndent, indent) << "}\n";
        }
    };

    std::stringstream stream;
    Write(*this, stream, 0);
    return stream.str();
}

DataFile& DataFile::operator[](const std::string& name)
{
    if (!_childIndexByName.contains(name))
    {
        _childIndexByName[name] = _children.size();
        _children.push_back(DataFile());
        _childrenNames.push_back(name);

        return _children.back();
    }

    return _children[_childIndexByName[name]];
}

const DataFile& DataFile::operator[](const std::string& name) const
{
    return at(name);
}

const DataFile& DataFile::at(const std::string& name) const				// Will throw if key not present.
{
    return _children[_childIndexByName.at(name)];
}

bool DataFile::HasChild(const std::string& name) const
{
    return _childIndexByName.contains(name);
}

void DataFile::Clear()
{
    _content.clear();
    _children.clear();
    _childrenNames.clear();
    _childIndexByName.clear();
}

bool DataFile::IsEmpty() const
{
    return _content.size() == 0 && _children.size() == 0;
}

int DataFile::GetValueCount() const
{
    return _content.size();
}

void DataFile::SetInt(int i, int index)
{
    SetString(std::to_string(i), index);
}

void DataFile::SetUInt(unsigned int i, int index)
{
    SetString(std::to_string(i), index);
}

void DataFile::SetULong(unsigned long long i, int index)
{
    SetString(std::to_string(i), index);
}

void DataFile::SetFloat(float f, int index)
{
    SetString(std::to_string(f), index);
}

void DataFile::CreateLeaf()
{
    // When used with the overloaded [] operators, this can be used
    // to construct an empty leaf node:
    // e.g. udf[Foo][Bar][Baz].CreateLeaf() creates the Foo -> Bar -> Baz
    // relationship, with Baz being an empty leaf node.
}

void DataFile::SetString(const std::string& str, int index)
{
    if (0 <= index)
    {
        if (index >= _content.size())
        {
            _content.resize(index + 1);
        }

        _content[index] = str;
    }
}

const std::string& DataFile::GetString(int index) const
{
    if (0 <= index && index < _content.size())
    {
        return _content[index];
    }

    return EMPTY_STRING;
}

int DataFile::GetInt(int index) const
{
    return std::stoi(GetString(index));
}

unsigned int DataFile::GetUInt(int index) const
{
    const unsigned long ul = std::stoul(GetString(index));
    assert(ul <= std::numeric_limits<unsigned int>::max());
    return static_cast<unsigned int>(ul);
}

unsigned long long DataFile::GetULong(int index) const
{
    return std::stoul(GetString(index));
}

float DataFile::GetFloat(int index) const
{
    return std::stof(GetString(index));
}

} // namespace Unalmas
