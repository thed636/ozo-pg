#pragma once

#include <ozo/pg/definitions.h>
#include <ozo/io/send.h>
#include <ozo/io/recv.h>

#include <boost/uuid/uuid.hpp>

#include <cstdint>

namespace ozo {

namespace detail {

// Boost 1.86 replaced the raw `uuid::data` array with a proxy type that is
// implicitly convertible to `std::uint8_t(&)[16]`. Binding through an explicit
// array reference works for both the old and the new representation, and keeps
// the raw-data stream overloads applicable.
using uuid_repr = std::uint8_t[16];

} // namespace detail

/**
 * @defgroup group-ext-boost-uuid boost::uuids::uuid
 * @ingroup group-ext-boost
 * @brief [boost::uuids::uuid](https://www.boost.org/doc/libs/1_62_0/libs/uuid/uuid.html) support
 *
 *@code
#include <ozo/ext/boost/uuid.h>
 *@endcode
 *
 * `boost::uuids::uuid` is defined as Universally Unique Identifierss data type.
 */

template <>
struct send_impl<boost::uuids::uuid> {
    template <typename OidMap>
    static ostream& apply(ostream& out, const OidMap&, const boost::uuids::uuid& in) {
        const detail::uuid_repr& repr = in.data;
        return write(out, repr);
    }
};

template <>
struct recv_impl<boost::uuids::uuid> {
    template <typename OidMap>
    static istream& apply(istream& in, size_type, const OidMap&, boost::uuids::uuid& out) {
        detail::uuid_repr& repr = out.data;
        return read(in, repr);
    }
};

}

OZO_PG_BIND_TYPE(boost::uuids::uuid, "uuid")
