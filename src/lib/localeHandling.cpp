/*
SPDX-FileCopyrightText: 2020 Simon A. Eugster <simon.eu@gmail.com>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "localeHandling.h"
#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtGlobal>
#include <clocale>

auto LocaleHandling::setLocale(const QString &lcName) -> QString
{
    QString newLocale;
    QList<QString> localesToTest;
    localesToTest << lcName << lcName + ".utf-8" << lcName + ".UTF-8" << lcName + ".utf8" << lcName + ".UTF8";
    for (const auto &locale : std::as_const(localesToTest)) {
#ifdef Q_OS_FREEBSD
        auto *result = setlocale(MLT_LC_CATEGORY, locale.toStdString().c_str());
#else
        auto *result = std::setlocale(MLT_LC_CATEGORY, locale.toStdString().c_str());
#endif
        if (result != nullptr) {
            ::qputenv(MLT_LC_NAME, locale.toStdString().c_str());
            newLocale = locale;
            break;
        }
    }
    if (newLocale.isEmpty()) {
        resetLocale();
    }
    return newLocale;
}

void LocaleHandling::resetLocale()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    std::setlocale(MLT_LC_CATEGORY, "en_US.UTF-8");
    ::qputenv(MLT_LC_NAME, "en_US.UTF-8");
#elif defined(Q_OS_FREEBSD)
    setlocale(MLT_LC_CATEGORY, "C");
    ::qputenv(MLT_LC_NAME, "C");
#else
    std::setlocale(MLT_LC_CATEGORY, "C");
    ::qputenv(MLT_LC_NAME, "C");
#endif
}

void LocaleHandling::resetAllLocale()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    std::setlocale(LC_ALL, "en_US.UTF-8");
    ::qputenv("LC_ALL", "en_US.UTF-8");
#elif defined Q_OS_FREEBSD
    setlocale(LC_ALL, "C.UTF-8");
    ::qputenv("LC_ALL", "C.UTF-8");
#else
    std::setlocale(LC_ALL, "C.UTF-8");
    ::qputenv("LC_ALL", "C.UTF-8");
#endif
}

QPair<QLocale, LocaleHandling::MatchType> LocaleHandling::getQLocaleForDecimalPoint(const QString &requestedLocale, const QString &decimalPoint)
{
    QLocale locale; // Best matching locale
    MatchType matchType = MatchType::NoMatch;

    // Parse installed locales to find one matching. Check matching language first
    QList<QLocale> list = QLocale::matchingLocales(QLocale().language(), QLocale().script(), QLocale::AnyCountry);
    for (const QLocale &loc : std::as_const(list)) {
        if (loc.decimalPoint() == decimalPoint) {
            locale = loc;
            matchType = MatchType::Exact;
            break;
        }
    }

    if (matchType == MatchType::NoMatch) {
        // Parse installed locales to find one matching. Check in all languages
        list = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale().script(), QLocale::AnyCountry);
        for (const QLocale &loc : std::as_const(list)) {
            if (loc.decimalPoint() == decimalPoint) {
                locale = loc;
                matchType = MatchType::DecimalOnly;
                break;
            }
        }
    }
    if (matchType == MatchType::NoMatch && requestedLocale == QLatin1String("C")) {
        locale = QLocale::c();
        matchType = MatchType::DecimalOnly;
    }
    return QPair<QLocale, LocaleHandling::MatchType>(locale, matchType);
}
