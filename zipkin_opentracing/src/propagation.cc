#include "propagation.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <zipkin/utility.h>

namespace ot = opentracing;

namespace zipkin {
#define PREFIX_TRACER_STATE "x-b3-"
// Note: these constants are a convention of the OpenTracing basictracers.
const ot::string_view prefix_baggage = "ot-baggage-";

const int tracer_state_required_field_count = 2;
const ot::string_view zipkin_trace_id = PREFIX_TRACER_STATE "traceid";
const ot::string_view zipkin_span_id = PREFIX_TRACER_STATE "spanid";
const ot::string_view zipkin_parent_span_id =
    PREFIX_TRACER_STATE "parentspanid";
const ot::string_view zipkin_sampled = PREFIX_TRACER_STATE "sampled";
const ot::string_view zipkin_flags = PREFIX_TRACER_STATE "flags";

const ot::string_view gcp_trace_ctx = "x-cloud-trace-context";

#undef PREFIX_TRACER_STATE

static bool keyCompare(ot::string_view lhs, ot::string_view rhs) {
  return lhs.length() == rhs.length() &&
         std::equal(
             std::begin(lhs), std::end(lhs), std::begin(rhs),
             [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

// Follows the rules of the go function strconv.ParseBool so as to interoperate
// with the other Zipkin tracing libraries.
// See https://golang.org/pkg/strconv/#ParseBool
static bool parseBool(ot::string_view s, bool &result) {
  if (s == "1" || s == "t" || s == "T" || s == "TRUE" || s == "true" ||
      s == "True") {
    result = true;
    return true;
  }
  if (s == "0" || s == "f" || s == "F" || s == "FALSE" || s == "false" ||
      s == "False") {
    result = false;
    return true;
  }
  return false;
}

opentracing::expected<void>
injectSpanContext(std::ostream &carrier,
                  const zipkin::SpanContext &span_context,
                  const std::unordered_map<std::string, std::string> &baggage) {
  return ot::make_unexpected(ot::invalid_carrier_error);
}

opentracing::expected<void>
injectSpanContext(const opentracing::TextMapWriter &carrier,
                  const zipkin::SpanContext &span_context,
                  const std::unordered_map<std::string, std::string> &baggage) {
  auto val = span_context.traceIdAsHexString() + "/" + std::to_string(span_context.id()) + ";o=" + (span_context.isSampled() ? "1" : "0");
  auto result = carrier.Set(gcp_trace_ctx, val);
  if (!result) {
    return result;
  }

  std::string baggage_key = prefix_baggage;
  for (const auto &baggage_item : baggage) {
    baggage_key.replace(std::begin(baggage_key) + prefix_baggage.size(),
                        std::end(baggage_key), baggage_item.first);
    result = carrier.Set(baggage_key, baggage_item.second);
    if (!result) {
      return result;
    }
  }
  return result;
}

opentracing::expected<Optional<zipkin::SpanContext>>
extractSpanContext(std::istream &carrier,
                   std::unordered_map<std::string, std::string> &baggage) {
  return ot::make_unexpected(ot::invalid_carrier_error);
}

opentracing::expected<Optional<zipkin::SpanContext>>
extractSpanContext(const opentracing::TextMapReader &carrier,
                   std::unordered_map<std::string, std::string> &baggage) {
  int required_field_count = 0;
  TraceId trace_id;
  Optional<TraceId> parent_id;
  uint64_t span_id;
  flags_t flags = 0;
  auto result = carrier.ForeachKey(
      [&](ot::string_view key, ot::string_view value) -> ot::expected<void> {
        if (keyCompare(key, gcp_trace_ctx)) {
          auto value_str = std::string{value};
          auto a = StringUtil::split(value_str, "/");
          auto b = StringUtil::split(a[1], ";");
          auto c = StringUtil::split(b[1], "=");

          auto trace_id_maybe = Hex::hexToTraceId(a[0]);
          if (!trace_id_maybe.valid()) {
            return ot::make_unexpected(ot::span_context_corrupted_error);
          }
          trace_id = trace_id_maybe.value();
          ++required_field_count;

          if (!StringUtil::atoull(b[0].c_str(), span_id)) {
            return ot::make_unexpected(ot::span_context_corrupted_error);
          }
          ++required_field_count;

          bool sampled;
          if (!parseBool(c[1], sampled)) {
            return ot::make_unexpected(ot::span_context_corrupted_error);
          }
          if (sampled) {
            flags |= sampled_flag;
          }
        } else if (key.length() > prefix_baggage.size() &&
                   keyCompare(
                       ot::string_view{key.data(), prefix_baggage.size()},
                       prefix_baggage)) {
          baggage.emplace(std::string{std::begin(key) + prefix_baggage.size(),
                                      std::end(key)},
                          value);
        }
        return {};
      });
  if (required_field_count == 0) {
    return {};
  }
  if (required_field_count != tracer_state_required_field_count) {
    return ot::make_unexpected(ot::span_context_corrupted_error);
  }
  return Optional<SpanContext>{
      SpanContext{trace_id, span_id, parent_id, flags}};
}
} // namespace zipkin
