// Copyright (c) 2015 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Authors: Lei He(helei@qiyi.com)

#ifndef BRPC_SPAN_EXPORTER_H
#define BRPC_SPAN_EXPORTER_H

#include <memory>
#include <ostream>
#include "butil/memory/singleton_on_pthread_once.h"
#include "butil/synchronization/lock.h"
#include "brpc/span.pb.h"

namespace brpc {

// You can customize the span's dumper mode by inheriting the class SpanDumper,
// Note:
// 1. It is not valid to call RegisterSpanExporter() again for the same 
// SpanExporter object multiple times. If you want to register same SpanExporter
// object for multiple times, each time you call RegisterSpanExporter(), you 
// should pass in a different SpanExporter object.
// 2. The order in which each SpanExporter::DumpSpan is called is independent 
// of the registration order
// 3. SpanDumper object should be managered by std::shared_ptr.
//
// Example:
//
// class FooSpanExporter: public brpc::SpanExporter {
// public:
//     void DumpSpan(const RpczSpan* span) {
//          // do somthing...
//     }
// };
//
// shared_ptr<FooSpanExporter> foo_span_dumper =
//     std::make_shared<brpc::FooSpanExporter>("FooSpanExporter");
// brpc::RegisterSpanExporter(foo_span_dumper);
//
    
class SpanExporter {
public:
    SpanExporter() {}

    SpanExporter(const std::string& name) : _name(name) {}

    virtual void DumpSpan(const TracingSpan* span) {}

    virtual void Describe(std::ostream& os) const;

    virtual ~SpanExporter() {}

private:
    std::string _name;
};

void RegisterSpanExporter(std::shared_ptr<SpanExporter> span_exporter);
void UnRegisterSpanExporter(std::shared_ptr<SpanExporter> span_exporter);

class Span;
class SpanExporterManager {
public:
    void RegisterSpanExporter(std::shared_ptr<SpanExporter> span_exporter);
    void UnRegisterSpanExporter(std::shared_ptr<SpanExporter> span_exporter);

private:
friend class butil::GetLeakySingleton<SpanExporterManager>;
friend class Span;
    void DumpSpan(const TracingSpan* span);

    DISALLOW_COPY_AND_ASSIGN(SpanExporterManager);

    SpanExporterManager() {}
    ~SpanExporterManager() {}

    std::set<std::shared_ptr<SpanExporter>> _exporter_set;
    butil::Mutex _exporter_set_mutex;
};

} // namespace brpc


#endif // BRPC_SPAN_EXPORTER_H
