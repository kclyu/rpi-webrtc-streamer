/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTILS_URL_H_
#define UTILS_URL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/httpcommon.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringutils.h"

namespace rtc {

static const uint16_t HTTP_DEFAULT_PORT = 80;
static const uint16_t HTTP_SECURE_PORT = 443;

//////////////////////////////////////////////////////////////////////
// Url
//////////////////////////////////////////////////////////////////////

template <class CTYPE>
class Url {
   public:
    typedef typename Traits<CTYPE>::string string;

    // TODO: Implement Encode/Decode
    static int Encode(const CTYPE* source, CTYPE* destination, size_t len);
    static int Encode(const string& source, string& destination);
    static int Decode(const CTYPE* source, CTYPE* destination, size_t len);
    static int Decode(const string& source, string& destination);

    Url(const string& url) { do_set_url(url.c_str(), url.size()); }
    Url(const string& path, const string& host,
        uint16_t port = HTTP_DEFAULT_PORT)
        : host_(host), port_(port), secure_(HTTP_SECURE_PORT == port) {
        set_full_path(path);
    }

    bool valid() const { return !host_.empty(); }
    void clear() {
        host_.clear();
        port_ = HTTP_DEFAULT_PORT;
        secure_ = false;
        path_.assign(1, static_cast<CTYPE>('/'));
        query_.clear();
    }

    void set_url(const string& val) { do_set_url(val.c_str(), val.size()); }
    string url() const {
        string val;
        do_get_url(&val);
        return val;
    }

    void set_address(const string& val) {
        do_set_address(val.c_str(), val.size());
    }
    string address() const {
        string val;
        do_get_address(&val);
        return val;
    }

    void set_full_path(const string& val) {
        do_set_full_path(val.c_str(), val.size());
    }
    string full_path() const {
        string val;
        do_get_full_path(&val);
        return val;
    }

    void set_host(const string& val) { host_ = val; }
    const string& host() const { return host_; }

    void set_port(uint16_t val) { port_ = val; }
    uint16_t port() const { return port_; }

    void set_secure(bool val) { secure_ = val; }
    bool secure() const { return secure_; }

    void set_path(const string& val) {
        if (val.empty()) {
            path_.assign(1, static_cast<CTYPE>('/'));
        } else {
            RTC_DCHECK(val[0] == static_cast<CTYPE>('/'));
            path_ = val;
        }
    }
    const string& path() const { return path_; }

    void set_query(const string& val) {
        RTC_DCHECK(val.empty() || (val[0] == static_cast<CTYPE>('?')));
        query_ = val;
    }
    const string& query() const { return query_; }

    bool get_attribute(const string& name, string* value) const;

   private:
    void do_set_url(const CTYPE* val, size_t len);
    void do_set_address(const CTYPE* val, size_t len);
    void do_set_full_path(const CTYPE* val, size_t len);

    void do_get_url(string* val) const;
    void do_get_address(string* val) const;
    void do_get_full_path(string* val) const;

    string host_, path_, query_;
    uint16_t port_;
    bool secure_;
};

}  // namespace rtc

#endif  // UTILS_URL_H_
