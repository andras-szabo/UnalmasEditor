#ifndef UNALMAS_GUID_H
#define UNALMAS_GUID_H

#include <functional>
namespace Unalmas
{
struct GUID
{
    static GUID Invalid();

    unsigned long long id;

    GUID();
    GUID(unsigned long long id_);
    GUID(const GUID& other) = default;
    GUID& operator=(const GUID& other) = default;
    GUID(GUID&& other) = default;
    GUID& operator=(GUID&& other) = default;

    bool operator==(const GUID& other) const;
    bool operator!=(const GUID& other) const;

    operator unsigned long long() const;
    bool IsValid() const;
};
} // namespace Unalmas

namespace std
{
template<>
struct hash<Unalmas::GUID>
{
    std::size_t operator()(const Unalmas::GUID& guid) const
    {
        return hash<unsigned long long>()(guid.id);
    }
};
} // namespace std

#endif // UNALMAS_GUID_H
