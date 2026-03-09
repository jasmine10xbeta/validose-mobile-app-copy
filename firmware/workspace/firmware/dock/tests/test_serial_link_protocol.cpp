// TODO: update calls to serial_link_protocol_serializer_init to handle the result.

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>

extern "C"
{
#include "common.h"

#include "serial_link_protocol.h"
#include "serial_link_protocol_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
class serial_link_test_suit: public testing::Test
{
protected:
   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

// TEST_F(serial_link_test_suit, serialize_ack_with_payload)
// {
//    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04}; // Arbitrary value for testing

//    link_layer_header_data_t header_data = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_ACK,
//       .packet_id = 0x1234,                 // Arbitrary value for testing
//       .payload_protocol_identifier = 0x23, // Arbitrary value for testing
//    };

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint8_t output_buffer[32];
//    memset(output_buffer, 0, sizeof(output_buffer));

//    uint16_t output_length;
//    result = serializer.interface.serialize(&serializer.interface,
//                                            &header_data,
//                                            payload,
//                                            sizeof(payload),
//                                            output_buffer,
//                                            sizeof(output_buffer),
//                                            &output_length);
//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(12 == output_length);

//    uint8_t expected_packet[] = {
//       0x01,
//       0x01,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x71,
//       0x8b,
//       0x00,
//       0x00,
//       0x04,
//    };

//    ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
// }

// TEST_F(serial_link_test_suit, serialize_ack_without_payload)
// {
//    uint8_t payload[] = {};

//    link_layer_header_data_t header_data = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_ACK,
//       .packet_id = 0x1234,
//       .payload_protocol_identifier = 0x23,
//    };

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint8_t output_buffer[32];
//    memset(output_buffer, 0, sizeof(output_buffer));

//    uint16_t output_length;
//    result = serializer.interface.serialize(&serializer.interface,
//                                            &header_data,
//                                            payload,
//                                            sizeof(payload),
//                                            output_buffer,
//                                            sizeof(output_buffer),
//                                            &output_length);
//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(12 == output_length);

//    uint8_t expected_packet[] = {
//       0x01,
//       0x01,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x71,
//       0x8b,
//       0x00,
//       0x00,
//       0x04,
//    };

//    ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
// }

// TEST_F(serial_link_test_suit, serialize_nak_with_payload)
// {
//    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

//    link_layer_header_data_t header_data = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_NAK,
//       .packet_id = 0x1234,
//       .payload_protocol_identifier = 0x23,
//    };

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint8_t output_buffer[32];
//    memset(output_buffer, 0, sizeof(output_buffer));

//    uint16_t output_length;
//    result = serializer.interface.serialize(&serializer.interface,
//                                            &header_data,
//                                            payload,
//                                            sizeof(payload),
//                                            output_buffer,
//                                            sizeof(output_buffer),
//                                            &output_length);
//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(12 == output_length);

//    uint8_t expected_packet[] = {
//       0x01,
//       0x02,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x0c,
//       0x87,
//       0x00,
//       0x00,
//       0x04,
//    };

//    ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
// }

// TEST_F(serial_link_test_suit, serialize_nak_without_payload)
// {
//    uint8_t payload[] = {};

//    link_layer_header_data_t header_data = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_NAK,
//       .packet_id = 0x1234,
//       .payload_protocol_identifier = 0x23,
//    };

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint8_t output_buffer[32];
//    memset(output_buffer, 0, sizeof(output_buffer));

//    uint16_t output_length;
//    result = serializer.interface.serialize(&serializer.interface,
//                                            &header_data,
//                                            payload,
//                                            sizeof(payload),
//                                            output_buffer,
//                                            sizeof(output_buffer),
//                                            &output_length);
//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(12 == output_length);

//    uint8_t expected_packet[] = {
//       0x01,
//       0x02,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x0c,
//       0x87,
//       0x00,
//       0x00,
//       0x04,
//    };

//    ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
// }

TEST_F(serial_link_test_suit, serialize_normal_payload)
{
   uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

   link_layer_header_data_t header_data = {
      .packet_type = LINK_LAYER_PACKET_TYPE_DATA,
      .packet_id = 0x1234,
      .payload_protocol_identifier = 0x23,
   };

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint8_t output_buffer[32];
   memset(output_buffer, 0, sizeof(output_buffer));

   uint16_t output_length;
   result = serializer.interface.serialize(&serializer.interface,
                                           &header_data,
                                           payload,
                                           sizeof(payload),
                                           output_buffer,
                                           sizeof(output_buffer),
                                           &output_length);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(16 == output_length);

   uint8_t expected_packet[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
}

TEST_F(serial_link_test_suit, serialize_empty_payload)
{
   uint8_t payload[] = {};

   link_layer_header_data_t header_data = {
      .packet_type = LINK_LAYER_PACKET_TYPE_DATA,
      .packet_id = 0x1234,
      .payload_protocol_identifier = 0x23,
   };

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint8_t output_buffer[32];
   memset(output_buffer, 0, sizeof(output_buffer));

   uint16_t output_length;
   result = serializer.interface.serialize(&serializer.interface,
                                           &header_data,
                                           payload,
                                           sizeof(payload),
                                           output_buffer,
                                           sizeof(output_buffer),
                                           &output_length);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(12 == output_length);

   uint8_t expected_packet[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x00,
      0x00,
      0x23,
      0x5a,
      0x8f,
      0x00,
      0x00,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(output_buffer, expected_packet, HEADER_LENGTH + TRAILER_LENGTH));
}

TEST_F(serial_link_test_suit, serialize_too_long_payload)
{
   uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

   link_layer_header_data_t header_data = {
      .packet_type = LINK_LAYER_PACKET_TYPE_DATA,
      .packet_id = 0x1234,
      .payload_protocol_identifier = 0x23,
   };

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint8_t output_buffer[15];
   memset(output_buffer, 0, sizeof(output_buffer));

   uint16_t output_length;
   result = serializer.interface.serialize(&serializer.interface,
                                           &header_data,
                                           payload,
                                           sizeof(payload),
                                           output_buffer,
                                           sizeof(output_buffer),
                                           &output_length);

   ASSERT_TRUE(SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL == GET_ERR_CODE(result));
}

// TEST_F(serial_link_test_suit, parse_ack)
// {
//    uint8_t packet_data[] = {
//       0x01,
//       0x01,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x71,
//       0x8b,
//       0x00,
//       0x00,
//       0x04,
//    };

//    link_layer_header_data_t header_data;
//    uint8_t payload[32];
//    uint16_t payload_length;

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint16_t prev_soh_pos = 0;

//    result = serializer.interface.parser_process(&serializer.interface,
//                                                 packet_data,
//                                                 sizeof(packet_data),
//                                                 &header_data,
//                                                 payload,
//                                                 sizeof(payload),
//                                                 &payload_length,
//                                                 &prev_soh_pos);

//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_ACK == header_data.packet_type);
//    ASSERT_TRUE(0x1234 == header_data.packet_id);
//    ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
//    ASSERT_TRUE(0 == payload_length);
// }

// TEST_F(serial_link_test_suit, parse_nak)
// {
//    uint8_t packet_data[] = {
//       0x01,
//       0x02,
//       0x34,
//       0x12,
//       0x00,
//       0x00,
//       0x23,
//       0x0c,
//       0x87,
//       0x00,
//       0x00,
//       0x04,
//    };

//    link_layer_header_data_t header_data;
//    uint8_t payload[32];
//    uint16_t payload_length;

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint16_t prev_soh_pos = 0;

//    result = serializer.interface.parser_process(&serializer.interface,
//                                                 packet_data,
//                                                 sizeof(packet_data),
//                                                 &header_data,
//                                                 payload,
//                                                 sizeof(payload),
//                                                 &payload_length,
//                                                 &prev_soh_pos);
//    ASSERT_TRUE(IS_OK(result));

//    ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_NAK == header_data.packet_type);
//    ASSERT_TRUE(0x1234 == header_data.packet_id);
//    ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
//    ASSERT_TRUE(0 == payload_length);
// }

TEST_F(serial_link_test_suit, parse_empty_payload)
{
   uint8_t packet_data[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x00,
      0x00,
      0x23,
      0x5a,
      0x8f,
      0x00,
      0x00,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(0 == payload_length);
}

TEST_F(serial_link_test_suit, parse_normal_payload)
{
   uint8_t packet_data[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_too_long_payload)
{
   uint8_t packet_data[] = {
      0x01, // SOH
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec, // EOH
      0x01, // SOP
      0x02,
      0x03,
      0x04, // EOP
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[2];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);

   ASSERT_TRUE(SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL == GET_ERR_CODE(result));
}

TEST_F(serial_link_test_suit, parse_garbage_before_bad_SOH_close_to_actual_SOH)
{
   // Test case where false SOH is within less than a HEADER_LENGTH of the actual SOH
   uint8_t packet_data[] = {
      0x00, 0x01, 0x23, 0x51, 0x12,                         // Garbage
      0x01, 0x00, 0x34, 0x12, 0x04, 0x00, 0x23, 0x3b, 0xec, // Header
      0x01, 0x02, 0x03, 0x04,                               // Payload
      0x4f, 0xc5, 0x04,                                     // Trailer
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);

   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_garbage_before_bad_SOH_far_from_actual_SOH)
{
   // Test case where false SOH is further than a HEADER_LENGTH of the actual SOH
   uint8_t packet_data[] = {
      0x00, 0x01, 0x23, 0x51, 0x12, 0x00, 0x23, 0x51, 0x12, 0x23, 0x51, 0x12, // Garbage
      0x01, 0x00, 0x34, 0x12, 0x04, 0x00, 0x23, 0x3b, 0xec,                   // Header
      0x01, 0x02, 0x03, 0x04,                                                 // Payload
      0x4f, 0xc5, 0x04,                                                       // Trailer
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);

   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_garbage_after)
{
   uint8_t packet_data[] = {
      0x01, 0x00, 0x34, 0x12, 0x04, 0x00, 0x23, 0x3b, 0xec, 0x01, 0x02,
      0x03, 0x04, 0x4f, 0xc5, 0x04, 0x00, 0x01, 0x23, 0x51, 0x12,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_corrupted_header)
{
   uint8_t packet_data[] = {
      0x01,
      0x00,
      0x01,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };
   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data,
                                                sizeof(packet_data),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);

   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));
}

// TEST_F(serial_link_test_suit, parse_corrupted_payload)
// {
//    uint8_t packet_data[] = {
//       0x01,
//       0x00,
//       0x34,
//       0x12,
//       0x04,
//       0x00,
//       0x23,
//       0x3b,
//       0xec,
//       0x01,
//       0x02,
//       0x05,
//       0x04,
//       0x4f,
//       0xc5,
//       0x04,
//    };

//    link_layer_header_data_t header_data;
//    uint8_t payload[32];
//    uint16_t payload_length;

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    uint16_t prev_soh_pos = 0;

//    result = serializer.interface.parser_process(&serializer.interface,
//                                                 packet_data,
//                                                 sizeof(packet_data),
//                                                 &header_data,
//                                                 payload,
//                                                 sizeof(payload),
//                                                 &payload_length,
//                                                 &prev_soh_pos);

//    ASSERT_TRUE(SERIALIZER_ERROR_CORRUPTED_PAYLOAD == GET_ERR_CODE(result));
// }

// TEST_F(serial_link_test_suit, parse_corrupted_trailer)
// {
//    uint8_t packet_data[] = {
//       0x01,
//       0x00,
//       0x34,
//       0x12,
//       0x04,
//       0x00,
//       0x23,
//       0x3b,
//       0xec,
//       0x01,
//       0x02,
//       0x03,
//       0x04,
//       0x4f,
//       0xc5,
//       0x05,
//    };

//    link_layer_header_data_t header_data;
//    uint8_t payload[32];
//    uint16_t payload_length;

//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));
//    uint16_t prev_soh_pos = 0;

//    result = serializer.interface.parser_process(&serializer.interface,
//                                                 packet_data,
//                                                 sizeof(packet_data),
//                                                 &header_data,
//                                                 payload,
//                                                 sizeof(payload),
//                                                 &payload_length,
//                                                 &prev_soh_pos);

//    ASSERT_TRUE(SERIALIZER_ERROR_CORRUPTED_PAYLOAD == GET_ERR_CODE(result));
// }

TEST_F(serial_link_test_suit, parse_normal_payload_split_over_two_packets_test1)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside header, before crc.
   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
   };
   uint8_t packet_data_2[] = {
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_normal_payload_split_over_two_packets_test2)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside header, in crc.

   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
   };
   uint8_t packet_data_2[] = {
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_truncated_packet_then_a_valid_packet1)
{
   // This test simulates receiving a partial packet from serial. 13 bytes of a 24 byte packet. Then receiving the
   // latter part of the packet from serial and calling parse_process a second time with new data from serial.

   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x1E,
      0x01,
      0x0C,
      0x00,
      0x02,
      0x27,
      0xD0,
      0x7D,
      0x0E,
      0x7C,
      0xBB,
   };
   uint8_t packet_data_2[] = {
      0x01, 0x00, 0x1E, 0x01, 0x0C, 0x00, 0x02, 0x27, 0xD0, 0x7D, 0x0E, 0x7C,
      0xBB, 0x9D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x69, 0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   printf("First packet evaluated %d \n", GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   printf("Error code %d \n", GET_ERR_CODE(result));
   ASSERT_TRUE(RESULT_OK == GET_ERR_CODE(result));
}

// 16 - 13
// 24 - 13

TEST_F(serial_link_test_suit, parse_truncated_packet_then_a_valid_packet2)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside header, in crc.

   uint8_t packet_data_1[] = {
      0x01, 0x00, 0x1E, 0x01, 0x0C, 0x00, 0x02, 0x27, 0xD0, 0x7D, 0x0E, 0x7C,
      0xBB, 0x9D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x69,
   };
   uint8_t packet_data_2[] = {
      0x01, 0x00, 0x1E, 0x01, 0x0C, 0x00, 0x02, 0x27, 0xD0, 0x7D, 0x0E, 0x7C,
      0xBB, 0x9D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x69, 0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   printf("Error code %d \n", GET_ERR_CODE(result));
   ASSERT_TRUE(RESULT_OK == GET_ERR_CODE(result));
}

TEST_F(serial_link_test_suit, parse_back_to_back_packets_where_the_first_misses_the_trailer_byte)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside header, in crc.

   uint8_t packet_data_1[] = {
      0x01, 0x00, 0x1E, 0x01, 0x0C, 0x00, 0x02, 0x27, 0xD0, 0x7D, 0x0E, 0x7C,
      0xBB, 0x9D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x69,
   };
   uint8_t packet_data_2[] = {
      0x01, 0x00, 0x1E, 0x01, 0x0C, 0x00, 0x02, 0x27, 0xD0, 0x7D, 0x0E, 0x7C,
      0xBB, 0x9D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x69, 0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   printf("Error code %d \n", GET_ERR_CODE(result));
   ASSERT_TRUE(RESULT_OK == GET_ERR_CODE(result));
}

TEST_F(serial_link_test_suit, parse_normal_payload_split_over_two_packets_test3)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside payload.

   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
   };
   uint8_t packet_data_2[] = {
      0x02,
      0x03,
      0x04,
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_normal_payload_split_over_two_packets_test4)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside trailer, before crc

   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
   };
   uint8_t packet_data_2[] = {
      0x4f,
      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, parse_normal_payload_split_over_two_packets_test5)
{
   // This test simulates receiving a partial packet from serial. Then receiving the latter part of the packet from
   // serial and calling parse_process a second time with new data from serial.
   // Split inside trailer, in CRC

   uint8_t packet_data_1[] = {
      0x01,
      0x00,
      0x34,
      0x12,
      0x04,
      0x00,
      0x23,
      0x3b,
      0xec,
      0x01,
      0x02,
      0x03,
      0x04,
      0x4f,
   };
   uint8_t packet_data_2[] = {

      0xc5,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
   ASSERT_TRUE(0x1234 == header_data.packet_id);
   ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
   ASSERT_TRUE(4 == payload_length);

   uint8_t expected_payload[] = {
      0x01,
      0x02,
      0x03,
      0x04,
   };

   ASSERT_EQ(0, std::memcmp(payload, expected_payload, payload_length));
}

TEST_F(serial_link_test_suit, test_parser_reset_function)
{
   // This test simulates receiving a partial packet from serial. 12 bytes of a 13 byte packet. Then the parser reset
   // function gets called, after which a valid packet gets sent. Before the EOT bug fix the first packet would have
   // caused the parser to get stuck. The reset function is a hard reset to clear the parser of such errors.

   uint8_t packet_data_1[] = {
      0x01, 0x00, 0xAF, 0x22, 0x01, 0x00, 0x00, 0x14, 0xF3, 0x05, 0xAD, 0x57,
      // 0x04,
   };
   uint8_t packet_data_2[] = {
      0x01,
      0x00,
      0xAF,
      0x22,
      0x01,
      0x00,
      0x00,
      0x14,
      0xF3,
      0x05,
      0xAD,
      0x57,
      0x04,
   };

   link_layer_header_data_t header_data;
   uint8_t payload[32];
   uint16_t payload_length;

   serial_link_protocol_serializer_t serializer;
   result_t result = serial_link_protocol_serializer_init(&serializer);
   ASSERT_TRUE(IS_OK(result));

   uint16_t prev_soh_pos = 0;

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_1,
                                                sizeof(packet_data_1),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   ASSERT_TRUE(SERIALIZER_ERROR_NO_FRAME == GET_ERR_CODE(result));

   printf("First packet evaluated %d \n", GET_ERR_CODE(result));

   (void)serializer.interface.parser_reset(&serializer.interface);

   result = serializer.interface.parser_process(&serializer.interface,
                                                packet_data_2,
                                                sizeof(packet_data_2),
                                                &header_data,
                                                payload,
                                                sizeof(payload),
                                                &payload_length,
                                                &prev_soh_pos);
   printf("Error code %d \n", GET_ERR_CODE(result));
   ASSERT_TRUE(RESULT_OK == GET_ERR_CODE(result));
}

// TEST_F(serial_link_test_suit, parse_2_normal_payloads_in_1_packet_test)
// {
//    // This test simulates receiving two normal payloads in one packet from serial. This test isn't really unique but
//    it
//    // does give an example of how to parse an incoming data stream chunks to ensure all valid frames are found.

//    // Init serializer
//    serial_link_protocol_serializer_t serializer;
//    result_t result = serial_link_protocol_serializer_init(&serializer);
//    ASSERT_TRUE(IS_OK(result));

//    // Create two frames
//    uint8_t payload1[] = {0x01, 0x02, 0x03, 0x04};
//    uint8_t payload2[] = {0x05, 0x06, 0x07};

//    link_layer_header_data_t header_data1 = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_DATA,
//       .packet_id = 0x0001,
//       .payload_protocol_identifier = 0x23,
//    };

//    link_layer_header_data_t header_data2 = {
//       .packet_type = LINK_LAYER_PACKET_TYPE_DATA,
//       .packet_id = 0x0002,
//       .payload_protocol_identifier = 0x23,
//    };

//    uint8_t frame1[32];
//    memset(frame1, 0, sizeof(frame1));

//    uint16_t frame1_length;

//    uint8_t frame2[32];
//    memset(frame2, 0, sizeof(frame2));

//    uint16_t frame2_length;

//    result = serializer.interface.serialize(
//       &serializer.interface, &header_data1, payload1, sizeof(payload1), frame1, sizeof(frame1), &frame1_length);
//    ASSERT_TRUE(IS_OK(result));
//    result = serializer.interface.serialize(
//       &serializer.interface, &header_data2, payload2, sizeof(payload2), frame2, sizeof(frame2), &frame2_length);
//    ASSERT_TRUE(IS_OK(result));

//    //---------------------------------------------------------------

//    // Compile mocked received serial data
//    uint8_t packet_data[100] = {0};

//    memcpy(&packet_data[5], frame1, frame1_length);
//    memcpy(&packet_data[40], frame2, frame2_length);

//    uint8_t serial_chunk_size = sizeof(packet_data);

//    // Parse incoming packet_data
//    link_layer_header_data_t header_data;
//    uint8_t payload[32] = {0};
//    uint16_t payload_length;

//    uint16_t cursor = 0;
//    uint16_t prev_soh_pos = 0;
//    uint8_t loop_counter = 0;

//    // Keep processing until all data has been processed.
//    while(cursor < serial_chunk_size)
//    {
//       result = serializer.interface.parser_process(&serializer.interface,
//                                                    &packet_data[cursor],
//                                                    serial_chunk_size - cursor,
//                                                    &header_data,
//                                                    payload,
//                                                    sizeof(payload),
//                                                    &payload_length,
//                                                    &prev_soh_pos);

//       switch(GET_ERR_CODE(result))
//       {
//          case SERIALIZER_ERROR_NONE:
//             // Valid frame detected. Process new frame.
//             if(0 == loop_counter)
//             {
//                ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
//                ASSERT_TRUE(0x0001 == header_data.packet_id);
//                ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
//                ASSERT_TRUE(4 == payload_length);

//                uint8_t expected_payload1[] = {
//                   0x01,
//                   0x02,
//                   0x03,
//                   0x04,
//                };
//                ASSERT_EQ(0, std::memcmp(payload, expected_payload1, payload_length));
//             }
//             else if(1 == loop_counter)
//             {
//                ASSERT_TRUE(LINK_LAYER_PACKET_TYPE_DATA == header_data.packet_type);
//                ASSERT_TRUE(0x0002 == header_data.packet_id);
//                ASSERT_TRUE(0x23 == header_data.payload_protocol_identifier);
//                ASSERT_TRUE(3 == payload_length);

//                uint8_t expected_payload2[] = {
//                   0x05,
//                   0x06,
//                   0x07,
//                };
//                ASSERT_EQ(0, std::memcmp(payload, expected_payload2, payload_length));
//             }

//             // Then continue to search for another frame
//             cursor += prev_soh_pos + HEADER_LENGTH + payload_length + TRAILER_LENGTH;

//             // Clear data for the next frame
//             header_data.packet_id = 0;
//             header_data.packet_type = LINK_LAYER_PACKET_TYPE_DATA;
//             header_data.payload_protocol_identifier = 0;
//             memset(payload, 0, sizeof(payload));
//             payload_length = 0;

//             loop_counter++; // To look for the second packet. Only used for this test
//             break;
//          case SERIALIZER_ERROR_NO_FRAME:
//             // The parser has gone through all the data in the chunk of serial data and found no more frames. Stop
//             the
//             // loop.
//             cursor = serial_chunk_size;
//             break;
//          case SERIALIZER_ERROR_CORRUPTED_PAYLOAD:
//             // Corrupted frame payload detected. Send NAK

//             cursor = prev_soh_pos + 1; // Advance cursor to keep looking for a frame
//             break;
//          case SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL:
//             ASSERT_TRUE(IS_OK(result)); // Fail if the buffer is too small
//             break;
//          default:
//          {
//             printf("Switch default triggered.");
//             ASSERT_TRUE(IS_OK(result)); // Fail
//          }
//       }
//    }
// }
