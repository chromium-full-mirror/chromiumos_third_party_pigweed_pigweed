// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

// TODO: https://pwbug.dev/452108892 - Remove this file.
// It is only a test to verify that the old template form of PwpbMetricWriter
// remains functional.

#include <limits>
#include <vector>

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_metric/metric_service_pwpb.h"
#include "pw_metric/pwpb_metric_writer.h"
#include "pw_metric_proto/metric_service.pwpb.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/encoder.h"
#include "pw_protobuf/serialized_size.h"
#include "pw_rpc/pwpb/test_method_context.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_rpc/test_helpers.h"
#include "pw_span/span.h"
#include "pw_stream/memory_stream.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/status_macros.h"

namespace pw::metric {
namespace {

// Helper function to precisely calculate the encoded size of a metric as it
// would appear in a WalkResponse. This is critical for setting up deterministic
// pagination tests.
size_t GetEncodedMetricSize(const Metric& metric, const Vector<Token>& path) {
  // 1) Calculate the size of the *nested* Metric message's payload.
  size_t metric_payload_size = 0;

  // The 'token_path' field is written as REPEATED (not packed).
  // Size = (tag + fixed32_value) * num_tokens
  metric_payload_size +=
      path.size() *
      protobuf::SizeOfFieldFixed32(proto::pwpb::Metric::Fields::kTokenPath);

  if (metric.is_float()) {
    metric_payload_size +=
        protobuf::SizeOfFieldFloat(proto::pwpb::Metric::Fields::kAsFloat);
  } else {
    metric_payload_size += protobuf::SizeOfFieldUint32(
        proto::pwpb::Metric::Fields::kAsInt, metric.as_int());
  }

  // 2) Calculate the size of the *entire* nested Metric message, including
  // its tag and length prefix, as a field in the parent WalkResponse.
  return protobuf::SizeOfDelimitedField(
      proto::pwpb::WalkResponse::Fields::kMetrics, metric_payload_size);
}

size_t CountMetricsInWalkResponse(ConstByteSpan serialized_response) {
  protobuf::Decoder decoder(serialized_response);
  size_t num_metrics = 0;
  while (decoder.Next().ok()) {
    if (decoder.FieldNumber() ==
        static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kMetrics)) {
      ++num_metrics;
    }
  }
  return num_metrics;
}

//
// PwpbMetricWriter Tests
//

TEST(PwpbMetricWriter, BasicWalk) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 123u);
  PW_METRIC(root, b, "b", 456.0f);
  PW_METRIC_GROUP(inner, "inner");
  PW_METRIC(inner, x, "x", 789u);
  root.Add(inner);

  std::array<std::byte, 256> encode_buffer;
  // Use WalkResponse as a mock parent message for testing the writer.
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  // Set limit to more than total metrics.
  size_t metric_limit = 5;

  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);

  MetricWalker walker(writer);

  Status walk_status = walker.Walk(root);
  PW_TEST_ASSERT_OK(walk_status);

  // The walk finished, so the status is OK.
  EXPECT_EQ(metric_limit, 2u);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            3u);
}

TEST(PwpbMetricWriter, StopsAtMetricLimit) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 123u);
  PW_METRIC(root, b, "b", 456.0f);
  PW_METRIC(root, x, "x", 789u);

  std::array<std::byte, 256> encode_buffer;
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  // Set limit to less than total metrics.
  size_t metric_limit = 2;

  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);

  MetricWalker walker(writer);

  // The writer will return ResourceExhausted when the limit hits 0, which
  // stops the walker.
  Status walk_status = walker.Walk(root);
  ASSERT_EQ(Status::ResourceExhausted(), walk_status);

  // Verify the limit was reached and the correct number of metrics were
  // written.
  EXPECT_EQ(metric_limit, 0u);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            2u);
}

TEST(PwpbMetricWriter, StopsAtBufferLimit) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, b, "b", 2u);
  PW_METRIC(root, c, "c", 3u);

  Vector<Token, 2> path_c;
  path_c.push_back(root.name());
  path_c.push_back(c.name());

  Vector<Token, 2> path_b;
  path_b.push_back(root.name());
  path_b.push_back(b.name());

  Vector<Token, 2> path_a;
  path_a.push_back(root.name());
  path_a.push_back(a.name());

  // Calculate the on-wire size of each metric as a repeated field.
  const size_t size_of_c = GetEncodedMetricSize(c, path_c);
  const size_t size_of_b = GetEncodedMetricSize(b, path_b);
  const size_t size_of_a = GetEncodedMetricSize(a, path_a);

  // All metrics have 2 tokens and a 1-byte int, so they should be equal.
  ASSERT_EQ(size_of_c, size_of_b);
  ASSERT_EQ(size_of_b, size_of_a);

  // We must also reserve space for the largest non-metric field that the parent
  // (WalkResponse) might write after the walk.
  constexpr size_t kWalkResponseOverhead =
      protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);

  // Our buffer needs to fit:
  // - The WalkResponse's non-metric field overhead
  // - Exactly 2 metrics (c and b)
  const size_t kSmallBufferSize = kWalkResponseOverhead + size_of_c + size_of_b;

  ASSERT_LT(kSmallBufferSize,
            (kWalkResponseOverhead + size_of_c + size_of_b + size_of_a));

  std::vector<std::byte> encode_buffer(kSmallBufferSize);
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  // Set a high limit so that the buffer is the constraint.
  size_t metric_limit = 10;

  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);

  MetricWalker walker(writer);

  Status walk_status = walker.Walk(root);
  ASSERT_EQ(Status::ResourceExhausted(), walk_status);

  // Verify that the metric limit was NOT the cause of the stop.
  // The walker will write 2 metrics (c and b) and stop before writing 'a'.
  EXPECT_EQ(metric_limit, 8u);
  size_t metrics_written = CountMetricsInWalkResponse(
      pw::span(parent_encoder.data(), parent_encoder.size()));
  EXPECT_GT(metrics_written, 0u);
  EXPECT_LT(metrics_written, 3u);
  EXPECT_EQ(metrics_written, 2u);
}

// Tests that the buffer limit is the constraint when the metric limit is
// set to "no limit" (i.e., SIZE_MAX).
TEST(PwpbMetricWriter, StopsAtBufferLimitWhenMetricLimitIsMax) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, b, "b", 2u);
  PW_METRIC(root, c, "c", 3u);

  Vector<Token, 2> path_c;
  path_c.push_back(root.name());
  path_c.push_back(c.name());

  Vector<Token, 2> path_b;
  path_b.push_back(root.name());
  path_b.push_back(b.name());

  Vector<Token, 2> path_a;
  path_a.push_back(root.name());
  path_a.push_back(a.name());

  const size_t size_of_c = GetEncodedMetricSize(c, path_c);
  const size_t size_of_b = GetEncodedMetricSize(b, path_b);
  const size_t size_of_a = GetEncodedMetricSize(a, path_a);
  ASSERT_EQ(size_of_c, size_of_b);
  ASSERT_EQ(size_of_b, size_of_a);

  constexpr size_t kWalkResponseOverhead =
      protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);

  // Set buffer size to fit exactly 2 metrics and overhead.
  const size_t kSmallBufferSize = kWalkResponseOverhead + size_of_c + size_of_b;
  std::vector<std::byte> encode_buffer(kSmallBufferSize);
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  // Set a "no limit" metric limit.
  size_t metric_limit = std::numeric_limits<size_t>::max();

  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);

  MetricWalker walker(writer);

  Status walk_status = walker.Walk(root);
  ASSERT_EQ(Status::ResourceExhausted(), walk_status);

  // Verify that the metric limit was NOT the cause of the stop.
  // The walker wrote 2 metrics (c and b) and decremented the limit.
  EXPECT_EQ(metric_limit, std::numeric_limits<size_t>::max() - 2);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            2u);
}

// Tests that the walker correctly does nothing (and returns OK) when
// walking a metric tree that has no metrics.
TEST(PwpbMetricWriter, WalksEmptyRoot) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC_GROUP(inner, "empty_child");
  root.Add(inner);

  std::array<std::byte, 256> encode_buffer;
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  size_t metric_limit = 5;
  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);
  MetricWalker walker(writer);

  Status walk_status = walker.Walk(root);
  PW_TEST_ASSERT_OK(walk_status);

  // No metrics were written, so limit is unchanged.
  EXPECT_EQ(metric_limit, 5u);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            0u);
}

// Tests that if a single metric is larger than the buffer, the walk
// immediately stops with RESOURCE_EXHAUSTED and writes 0 metrics.
TEST(PwpbMetricWriter, StopsWhenSingleMetricIsTooLarge) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a_metric_with_a_name", 1u);

  // Calculate the true size of this metric.
  Vector<Token, 2> path_a;
  path_a.push_back(root.name());
  path_a.push_back(a.name());
  const size_t size_of_a = GetEncodedMetricSize(a, path_a);
  ASSERT_GT(size_of_a, 10u);

  // Create a buffer that is smaller than that single metric.
  const size_t kTooSmallBufferSize = size_of_a - 1;
  std::vector<std::byte> encode_buffer(kTooSmallBufferSize);
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  size_t metric_limit = 10;
  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);
  MetricWalker walker(writer);

  // The first call to Write() should fail.
  Status walk_status = walker.Walk(root);
  ASSERT_EQ(Status::ResourceExhausted(), walk_status);

  // No metrics were written, limit is unchanged.
  EXPECT_EQ(metric_limit, 10u);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            0u);
}

// Tests that the sizing logic is correct for mixed float and int value types.
TEST(PwpbMetricWriter, WalksWithMixedTypesAndExactBuffer) {
  PW_METRIC_GROUP(root, "/");
  // Walk order will be: float_metric, int_metric
  PW_METRIC(root, int_metric, "int_metric", 123u);
  PW_METRIC(root, float_metric, "float_metric", 456.0f);

  Vector<Token, 2> path_int;
  path_int.push_back(root.name());
  path_int.push_back(int_metric.name());
  const size_t size_of_int = GetEncodedMetricSize(int_metric, path_int);

  Vector<Token, 2> path_float;
  path_float.push_back(root.name());
  path_float.push_back(float_metric.name());
  const size_t size_of_float = GetEncodedMetricSize(float_metric, path_float);

  // This test relies on the float (fixed32) being larger than the int (varint).
  // (e.g., int_metric=123u takes 2 bytes; float takes 5 bytes).
  ASSERT_GT(size_of_float, size_of_int);

  // We must also reserve space for the largest non-metric field.
  constexpr size_t kWalkResponseOverhead =
      protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);

  // Create a buffer that fits exactly these two metrics and overhead.
  const size_t kExactBufferSize =
      size_of_float + size_of_int + kWalkResponseOverhead;

  std::vector<std::byte> encode_buffer(kExactBufferSize);
  proto::pwpb::WalkResponse::MemoryEncoder parent_encoder(encode_buffer);

  size_t metric_limit = 10;
  PwpbMetricWriter<proto::pwpb::WalkResponse::MemoryEncoder,
                   static_cast<uint32_t>(
                       proto::pwpb::WalkResponse::Fields::kMetrics)>
      writer(parent_encoder, metric_limit);
  MetricWalker walker(writer);

  Status walk_status = walker.Walk(root);
  PW_TEST_ASSERT_OK(walk_status);

  // 2 metrics were written.
  EXPECT_EQ(metric_limit, 8u);
  EXPECT_EQ(CountMetricsInWalkResponse(
                pw::span(parent_encoder.data(), parent_encoder.size())),
            2u);
}

}  // namespace
}  // namespace pw::metric
