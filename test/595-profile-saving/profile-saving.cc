/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dex/dex_file.h"

#include "art_method-inl.h"
#include "dex/method_reference.h"
#include "jit/profile_saver.h"
#include "jni.h"
#include "mirror/class-inl.h"
#include "mirror/executable.h"
#include "nativehelper/ScopedUtfChars.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "profile/profile_compilation_info.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace {

extern "C" JNIEXPORT void JNICALL Java_Main_ensureProfilingInfo(JNIEnv* env,
                                                                jclass,
                                                                jobject method) {
  CHECK(method != nullptr);
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Executable> exec = soa.Decode<mirror::Executable>(method);
  ArtMethod* art_method = exec->GetArtMethod();
  if (ProfilingInfo::Create(soa.Self(), art_method) == nullptr) {
    LOG(ERROR) << "Failed to create profiling info for method " << art_method->PrettyMethod();
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_ensureProfileProcessing(JNIEnv*, jclass) {
  ProfileSaver::ForceProcessProfiles();
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isForBootImage(JNIEnv* env,
                                                               jclass,
                                                               jstring filename) {
  ScopedUtfChars filename_chars(env, filename);
  CHECK(filename_chars.c_str() != nullptr);

  ProfileCompilationInfo info(/*for_boot_image=*/ true);
  bool result = info.Load(std::string(filename_chars.c_str()), /*clear_if_invalid=*/ false);
  return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_presentInProfile(JNIEnv* env,
                                                                 jclass c,
                                                                 jstring filename,
                                                                 jobject method) {
  bool for_boot_image = Java_Main_isForBootImage(env, c, filename) == JNI_TRUE;
  ScopedUtfChars filename_chars(env, filename);
  CHECK(filename_chars.c_str() != nullptr);
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Executable> exec = soa.Decode<mirror::Executable>(method);
  ArtMethod* art_method = exec->GetArtMethod();
  MethodReference ref(art_method->GetDexFile(), art_method->GetDexMethodIndex());

  ProfileCompilationInfo info(Runtime::Current()->GetArenaPool(), for_boot_image);
  if (!info.Load(filename_chars.c_str(), /*clear_if_invalid=*/false)) {
    LOG(ERROR) << "Failed to load profile from " << filename;
    return JNI_FALSE;
  }
  const ProfileCompilationInfo::MethodHotness hotness = info.GetMethodHotness(ref);
  // TODO: Why do we check `hotness.IsHot()` instead of `hotness.IsInProfile()`
  // in a method named `presentInProfile()`?
  return hotness.IsHot() ? JNI_TRUE : JNI_FALSE;
}

}  // namespace
}  // namespace art
