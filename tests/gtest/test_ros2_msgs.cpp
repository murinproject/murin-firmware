#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {
#include "framed_link.h"
#include "ros2_msgs.h"
}

namespace {

constexpr uint8_t kHeartbeat = 0x00;
constexpr uint8_t kMotorCommand = 0x01;
constexpr uint8_t kAck = FRAMED_LINK_MSG_ACK;
constexpr uint8_t kNack = FRAMED_LINK_MSG_NACK;
constexpr uint8_t kErrorLength = FRAMED_LINK_ERR_LEN;
constexpr uint8_t kErrorType = 0x03;

struct WireCapture {
    uint8_t bytes[1024]{};
    size_t length = 0;
};

size_t CaptureWrite(void *context, uint8_t *data, size_t length)
{
    auto *capture = static_cast<WireCapture *>(context);
    if (length > sizeof(capture->bytes)) {
        return 0;
    }

    std::memcpy(capture->bytes, data, length);
    capture->length = length;
    return length;
}

WireCapture *g_ros_response = nullptr;

size_t CaptureRosWrite(void *context, uint8_t *data, size_t length)
{
    (void)context;
    return CaptureWrite(g_ros_response, data, length);
}

struct DecodedFrame {
    uint8_t type = 0;
    uint8_t sequence = 0;
    uint8_t payload[256]{};
    size_t payload_length = 0;
    unsigned int count = 0;
};

void CaptureFrame(void *context, uint8_t type, uint8_t sequence,
                  const uint8_t *payload, size_t payload_length)
{
    auto *frame = static_cast<DecodedFrame *>(context);
    frame->type = type;
    frame->sequence = sequence;
    frame->payload_length = payload_length;
    std::memcpy(frame->payload, payload, payload_length);
    ++frame->count;
}

DecodedFrame Decode(const WireCapture &wire)
{
    DecodedFrame decoded;
    framed_link_t parser{};
    framed_link_init(&parser, nullptr, nullptr, CaptureFrame, &decoded);

    uint8_t bytes[sizeof(wire.bytes)];
    std::memcpy(bytes, wire.bytes, wire.length);
    framed_link_process(&parser, bytes, wire.length);
    return decoded;
}

class Ros2MsgsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_ros_response = &response_;
        messages_.write = CaptureRosWrite;
        ros2_msgs_init();
    }

    void TearDown() override { g_ros_response = nullptr; }

    DecodedFrame SendToRos(uint8_t type, uint8_t sequence,
                           const uint8_t *payload, size_t payload_length)
    {
        WireCapture request;
        framed_link_t host_link{};
        framed_link_init(&host_link, CaptureWrite, &request, nullptr, nullptr);
        framed_link_send_frame(&host_link, type, sequence, payload, payload_length);

        framed_link_process(&messages_.link, request.bytes, request.length);
        return Decode(response_);
    }

    ros2_msgs_ctx_t messages_{};
    WireCapture response_{};
};

TEST_F(Ros2MsgsTest, SendFramePreservesFieldsAndEscapesPayload)
{
    const uint8_t payload[] = {0xAA, 0x1B, 0x00, 0x55};

    ros2_msgs_send_frame(&messages_, 0x42, 17, payload, sizeof(payload));

    const DecodedFrame decoded = Decode(response_);
    ASSERT_EQ(decoded.count, 1u);
    EXPECT_EQ(decoded.type, 0x42);
    EXPECT_EQ(decoded.sequence, 17);
    ASSERT_EQ(decoded.payload_length, sizeof(payload));
    EXPECT_EQ(std::memcmp(decoded.payload, payload, sizeof(payload)), 0);
}

TEST_F(Ros2MsgsTest, HeartbeatReceivesAcknowledgement)
{
    const DecodedFrame response = SendToRos(kHeartbeat, 21, nullptr, 0);

    ASSERT_EQ(response.count, 1u);
    EXPECT_EQ(response.type, kAck);
    EXPECT_EQ(response.sequence, 21);
    ASSERT_EQ(response.payload_length, 1u);
    EXPECT_EQ(response.payload[0], 21);
}

TEST_F(Ros2MsgsTest, ValidMotorCommandReceivesAcknowledgement)
{
    const uint8_t payload[] = {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0xBF};
    const DecodedFrame response = SendToRos(kMotorCommand, 22, payload, sizeof(payload));

    ASSERT_EQ(response.count, 1u);
    EXPECT_EQ(response.type, kAck);
    EXPECT_EQ(response.sequence, 22);
}

TEST_F(Ros2MsgsTest, MalformedMotorCommandReceivesLengthNack)
{
    const uint8_t payload[] = {0x34, 0x12, 0x78};
    const DecodedFrame response = SendToRos(kMotorCommand, 23, payload, sizeof(payload));

    ASSERT_EQ(response.count, 1u);
    EXPECT_EQ(response.type, kNack);
    EXPECT_EQ(response.sequence, 23);
    ASSERT_EQ(response.payload_length, 2u);
    EXPECT_EQ(response.payload[0], 23);
    EXPECT_EQ(response.payload[1], kErrorLength);
}

TEST_F(Ros2MsgsTest, UnknownMessageReceivesTypeNack)
{
    const DecodedFrame response = SendToRos(0xFE, 24, nullptr, 0);

    ASSERT_EQ(response.count, 1u);
    EXPECT_EQ(response.type, kNack);
    EXPECT_EQ(response.sequence, 24);
    ASSERT_EQ(response.payload_length, 2u);
    EXPECT_EQ(response.payload[0], 24);
    EXPECT_EQ(response.payload[1], kErrorType);
}

}  // namespace
