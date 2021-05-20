/*
   Copyright (c) 2016, The CyanogenMod Project
   Copyright (c) 2017, The LineageOS Project

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdlib>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include "property_service.h"
#include "vendor_init.h"

using android::base::GetProperty;

typedef struct {
    char const *heapstartsize;
    char const *heapgrowthlimit;
    char const *heapsize;
    char const *heapminfree;
    char const *heapmaxfree;
} dalvikprop_t;

/* From Magisk@jni/magiskhide/hide_utils.c */
static const char *snet_prop_key[] = {
  "ro.boot.vbmeta.device_state",
  "ro.boot.verifiedbootstate", 
  "ro.boot.flash.locked",
  "ro.boot.selinux",
  "ro.boot.veritymode",
  "ro.boot.warranty_bit",
  "ro.warranty_bit",
  "ro.debuggable",
  "ro.secure",
  "ro.build.type",
  "ro.build.tags",
  "ro.build.selinux",
  NULL
};

static const char *snet_prop_value[] = {
  "locked",
  "green",
  "1",
  "enforcing",
  "enforcing",
  "0",
  "0",
  "0",
  "1",
  "user",
  "release-keys",
  "1",
  NULL
};

static void property_override(char const prop[], char const value[]) {
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_dual(char const system_prop[], char const vendor_prop[], const char value[])
{
        property_override(system_prop, value);
        property_override(vendor_prop, value);
}

static void check_device(dalvikprop_t* dprop) {
    struct sysinfo sys;

    sysinfo(&sys);

    if (dprop == nullptr)
        return;

    if (sys.totalram > 3072ull * 1024 * 1024) {
        // from - phone-xxhdpi-4096-dalvik-heap.mk
        dprop->heapstartsize = "16m";
        dprop->heapgrowthlimit = "256m";
        dprop->heapsize = "512m";
        dprop->heapminfree = "4m";
        dprop->heapmaxfree = "8m";
    } else if (sys.totalram > 2048ull * 1024 * 1024) {
        // from - phone-xxhdpi-3072-dalvik-heap.mk
        dprop->heapstartsize = "8m";
        dprop->heapgrowthlimit = "288m";
        dprop->heapsize = "768m";
        dprop->heapminfree = "512k";
        dprop->heapmaxfree = "8m";
    } else {
        // from - phone-xxhdpi-2048-dalvik-heap.mk
        dprop->heapstartsize = "16m";
        dprop->heapgrowthlimit = "192m";
        dprop->heapsize = "512m";
        dprop->heapminfree = "2m";
        dprop->heapmaxfree = "8m";
   }
}

static void init_target_properties() {
    std::ifstream fin;
    std::string buf;

    std::string product = GetProperty("ro.product.name", "");
    if (product.find("santoni") == std::string::npos)
        return;

    fin.open("/proc/cmdline");
    while (std::getline(fin, buf, ' '))
        if (buf.find("board_id") != std::string::npos)
            break;
    fin.close();

    if (buf.find("S88536CA2") != std::string::npos) {
        property_override("ro.product.model", "Redmi 4");
        property_override("ro.product.vendor.model", "Redmi 4");
    }
}

static void workaround_snet_properties() {

  // Hide all sensitive props
  for (int i = 0; snet_prop_key[i]; ++i) {
    property_override(snet_prop_key[i], snet_prop_value[i]);
  }

  chmod("/sys/fs/selinux/enforce", 0640);
  chmod("/sys/fs/selinux/policy", 0440);
}

void vendor_load_properties() {
    dalvikprop_t dprop;
    check_device(&dprop);

    property_override("dalvik.vm.heapstartsize", dprop.heapstartsize);
    property_override("dalvik.vm.heapgrowthlimit", dprop.heapgrowthlimit);
    property_override("dalvik.vm.heapsize", dprop.heapsize);
    property_override("dalvik.vm.heaptargetutilization", "0.75");
    property_override("dalvik.vm.heapminfree", dprop.heapminfree);
    property_override("dalvik.vm.heapmaxfree", dprop.heapmaxfree);
    property_override_dual("ro.control_privapp_permissions", "ro.control_privapp_permissions", "log");
    property_override_dual("ro.system.build.fingerprint", "ro.vendor.build.fingerprint", "google/sunfish/sunfish:11/RQ2A.210505.002/7246365:user/release-keys");
    property_override_dual("ro.build.fingerprint", "ro.product.build.fingerprint", "google/sunfish/sunfish:11/RQ2A.210505.002/7246365:user/release-keys");

    init_target_properties();

// Workaround SafetyNet
workaround_snet_properties();

}
