// @file shadow.h
/// @brief Secret password file routines
#pragma once

#include "stddef.h"

#define SHADOW "/etc/shadow"

/// @brief Stores password information.
struct spwd {
    char *sp_namp;             ///< user login name.
    char *sp_pwdp;             ///< encrypted password.
    long int sp_lstchg;        ///< last password change.
    long int sp_min;           ///< days until change allowed.
    long int sp_max;           ///< days before change required.
    long int sp_warn;          ///< days warning for expiration.
    long int sp_inact;         ///< days before account inactive.
    long int sp_expire;        ///< date when account expires.
    unsigned long int sp_flag; ///< reserved for future use.
};

/// @brief returns a pointer to a structure containing the broken-out fields of
/// the record in the shadow password database that matches the username name.
/// @param name the name to search in the database.
/// @return returns NULL if there are no more entries or an error accured.
struct spwd *getspnam(const char *name);

/// @brief Like getspnam() but stores the retrieved shadow password structure in
/// the space pointed to by spbuf.
/// @param name the name to search in the database.
/// @param spbuf where it stores the retrieved shadow password information.
/// @param buf spbuf contains pointers to strings, and these strings are stored
/// in the buffer buf of size buflen.
/// @param buflen size of buflen.
/// @param spbufp contains a pointer to the result or NULL.
/// @return returns zero on success, otherwise a (positive) error number is returned.
int getspnam_r(const char *name, struct spwd *spbuf, char buf[], size_t buflen, struct spwd **spbufp);
