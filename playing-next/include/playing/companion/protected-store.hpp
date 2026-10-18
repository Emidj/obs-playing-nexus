// SPDX-License-Identifier: MIT
#pragma once
#include <QByteArray>
#include <QString>
#include <optional>
namespace playing::companion {
// User-scoped Windows DPAPI, opaque payload <=64 KiB; config schema belongs to
// the app host. Explicit absolute app-owned path, atomic save, no plaintext disk.
// Does not make secrets exportable to a different user's/machine's profile.
class ProtectedStore final {
public:
    explicit ProtectedStore(QString absolutePath);
    void save(const QByteArray &payload) const;
    std::optional<QByteArray> load() const; // Missing=nullopt, corrupt/access=exception.
private:
    QString path_;
};
} // namespace playing::companion
