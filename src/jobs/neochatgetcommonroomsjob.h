// SPDX-FileCopyrightText: 2024 Joshua Goins <josh@redstrate.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Quotient/jobs/basejob.h>
#include <Quotient/omittable.h>

// TODO: Upstream to libQuotient
class NeochatGetCommonRoomsJob : public Quotient::BaseJob
{
public:
    explicit NeochatGetCommonRoomsJob(const QString &userId, const Quotient::Omittable<QJsonObject> &auth = {});
};
