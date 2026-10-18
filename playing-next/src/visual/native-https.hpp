// SPDX-License-Identifier: MIT
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <QByteArray>
#include <QUrl>
#include <chrono>
namespace playing::visual {
struct HttpReply {DWORD status=0;QByteArray body;};
inline HttpReply httpsGet(const QUrl &url,const QString &headers={},qint64 limit=4*1024*1024){
    if(url.scheme()!="https"||url.host().isEmpty()||!url.userInfo().isEmpty()||!url.fragment().isEmpty()||url.toEncoded().size()>4096)return {};
    struct Handle{HINTERNET value;~Handle(){if(value)WinHttpCloseHandle(value);}};
    Handle session{WinHttpOpen(L"OBS Playing Next",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};if(!session.value)return {};
    WinHttpSetTimeouts(session.value,2000,2000,2000,2000);
    const auto host=url.host().toStdWString();Handle connection{WinHttpConnect(session.value,host.c_str(),static_cast<INTERNET_PORT>(url.port(443)),0)};if(!connection.value)return {};
    const auto path=(url.path(QUrl::FullyEncoded)+(url.hasQuery()?"?"+url.query(QUrl::FullyEncoded):QString{})).toStdWString();
    Handle request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.value)return {};
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,autologon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&autologon,sizeof(autologon));
    const auto fields=headers.toStdWString();if(!WinHttpSendRequest(request.value,fields.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:fields.c_str(),fields.empty()?0:static_cast<DWORD>(fields.size()),nullptr,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))return {};
    HttpReply result;DWORD size=sizeof(result.status);if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&result.status,&size,WINHTTP_NO_HEADER_INDEX)||result.status!=200)return result;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);char buffer[32768];DWORD count=0;
    for(;;){if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&count))return {};if(!count)break;if(std::chrono::steady_clock::now()>deadline||result.body.size()+count>limit)return {};result.body.append(buffer,static_cast<int>(count));}return result;
}
}
