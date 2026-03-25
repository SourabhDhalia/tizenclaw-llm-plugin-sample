Name:       tizenclaw-llm-plugin-sample
Summary:    TizenClaw Sample LLM Backend Plugin
Version:    1.0.0
Release:    1
Group:      System/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: tizen-manifest.xml
Source1002: %{name}.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(tizenclaw-core)
BuildRequires:  pkgconfig(tizen-core)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(libwebsockets)
BuildRequires:  pkgconfig(pkgmgr)
BuildRequires:  pkgconfig(pkgmgr-info)
BuildRequires:  pkgconfig(pkgmgr-installer)
BuildRequires:  pkgconfig(pkgmgr-parser)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(capi-appfw-tizen-action)

%description
Sample LLM Backend for TizenClaw

%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1001} .
cp %{SOURCE1002} .

%build
%cmake .
%__make %{?_smp_mflags}

%install
%make_install
mkdir -p %{buildroot}/usr/apps/org.tizen.tizenclaw-llm-plugin-sample
mkdir -p %{buildroot}/usr/share/packages
cp tizen-manifest.xml %{buildroot}/usr/share/packages/org.tizen.tizenclaw-llm-plugin-sample.xml
cp tizen-manifest.xml %{buildroot}/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/tizen-manifest.xml

# Signing
%define tizen_sign_base %{_prefix}/apps/org.tizen.tizenclaw-llm-plugin-sample
%define tizen_sign 1
%define tizen_author_sign 1
%define tizen_dist_sign 1
%define tizen_sign_level platform

%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
/usr/share/packages/org.tizen.tizenclaw-llm-plugin-sample.xml
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/tizen-manifest.xml
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/res/plugin_llm_config.json
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/res/mcp_servers_shopping.json
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/res/agent_role_shopping.json
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/res/autonomous_trigger_shopping.json
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/lib/libplugin-sample.so
/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/bin/mcp-http-bridge
