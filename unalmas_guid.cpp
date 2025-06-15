#include "unalmas_guid.h"
#include <random>

namespace Unalmas
{
static std::random_device randomDevice;
static std::mt19937_64 engine(randomDevice());
static std::uniform_int_distribution<unsigned long long> randomDistribution;

GUID::GUID(): id { randomDistribution(engine) }
{}

GUID::GUID(unsigned long long id_): id {id_}
{}

GUID GUID::Invalid()
{
    return GUID(0);
}

GUID::operator unsigned long long() const
{
    return id;
}

bool GUID::operator==(const GUID& other) const
{
    return other.id == id;
}

bool GUID::operator!=(const GUID& other) const
{
    return other.id != id;
}

bool GUID::IsValid() const
{
    return id != 0;
}


} // namespace Unalmas
