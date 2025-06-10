#ifndef UNALMAS_DATAFILE_H
#define UNALMAS_DATAFILE_H

#include <string>
#include <vector>
#include <unordered_map>

namespace Unalmas
{

constexpr std::string EMPTY_STRING {""};

class DataFile
{
public:
    static const std::string EscapeString(const std::string& str, const char separator);
    static bool Serialize(const DataFile& data,
                          const std::string& fileName,
                          const std::string& indent = "  ",
                          const char separator = ',');
    static DataFile Deserialize(const std::string& fileName, const char separator = ',');
    static DataFile FromStream(std::istream& stream, const char separator = ',');
    static DataFile FromString(const std::string& asString, const char separator = ',');

    DataFile() = default;
    explicit DataFile(std::size_t expectedChildrenCount);

    bool IsEmpty() const;

    inline void SetString(const std::string& str, int index = 0);
    inline void SetInt(int i, int index = 0);
    inline void SetUInt(unsigned int i, int index = 0);
    inline void SetULong(unsigned long long ulong, int index = 0);
    inline void SetFloat(float f, int index = 0);
    inline void CreateLeaf();

    inline const std::string& GetString(int index = 0) const;
    inline int GetInt(int index = 0) const;
    inline unsigned int GetUInt(int index = 0) const;
    inline unsigned long long GetULong(int index =0) const;
    inline float GetFloat(int index = 0) const;
    inline int GetValueCount() const;

    void Clear();

    std::string ToString(const std::string& indent = "  ", const char separator = ',') const;

    inline DataFile& operator[](const std::string& name);
    inline const DataFile& operator[](const std::string& name) const;
    inline const DataFile& at(const std::string& name) const;
    inline bool HasChild(const std::string& name) const;

private:
    std::vector<std::string> _content;
    std::vector<DataFile> _children;
    std::vector<std::string> _childrenNames;
    std::unordered_map<std::string, int,
                       std::hash<std::string>,
                       std::equal_to<std::string>> _childIndexByName;
};

} // namespace Unalmas
#endif // UNALMAS_DATAFILE_H
