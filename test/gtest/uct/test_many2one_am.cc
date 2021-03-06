/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2015.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#include "uct_test.h"

extern "C" {
#include <ucs/arch/atomic.h>
}

class test_many2one_am : public uct_test {
public:
    static const uint8_t  AM_ID = 15;
    static const uint64_t MAGIC = 0xdeadbeef12345678ul;

    typedef struct {
        uint64_t magic;
        unsigned length;
    } receive_desc_t;

    test_many2one_am() : m_am_count(0) {
    }

    static ucs_status_t am_handler(void *arg, void *data, size_t length, void *desc) {
        test_many2one_am *self = reinterpret_cast<test_many2one_am*>(arg);
        return self->am_handler(data, length, desc);
    }

    ucs_status_t am_handler(void *data, size_t length, void *desc) {
        if (rand() % 4 == 0) {
            receive_desc_t *my_desc = (receive_desc_t *)desc;
            my_desc->magic  = MAGIC;
            my_desc->length = length;
            if (data != my_desc + 1) {
                memcpy(my_desc + 1, data, length);
            }
            m_backlog.push_back(my_desc);
            ucs_atomic_add32(&m_am_count, 1);
            return UCS_INPROGRESS;
        }
        mapped_buffer::pattern_check(data, length);
        ucs_atomic_add32(&m_am_count, 1);
        return UCS_OK;
    }

    void check_backlog() {
        while (!m_backlog.empty()) {
            receive_desc_t *my_desc = m_backlog.back();
            m_backlog.pop_back();
            EXPECT_EQ(uint64_t(MAGIC), my_desc->magic);
            mapped_buffer::pattern_check(my_desc + 1, my_desc->length);
            uct_iface_release_am_desc(my_desc);
        }
    }

    static const size_t NUM_SENDERS = 10;

protected:
    volatile uint32_t            m_am_count;
    std::vector<receive_desc_t*> m_backlog;
};


UCS_TEST_P(test_many2one_am, am_bcopy, "MAX_BCOPY=16384")
{
    const unsigned num_sends = 1000 / ucs::test_time_multiplier();
    ucs_status_t status;

    entity *receiver = create_entity(sizeof(receive_desc_t));
    m_entities.push_back(receiver);

    check_caps(UCT_IFACE_FLAG_AM_BCOPY);
    check_caps(UCT_IFACE_FLAG_AM_CB_SYNC);

    ucs::ptr_vector<entity> senders;
    ucs::ptr_vector<mapped_buffer> buffers;
    for (unsigned i = 0; i < NUM_SENDERS; ++i) {
        entity *sender = create_entity(0);
        mapped_buffer *buffer = new mapped_buffer(
                            sender->iface_attr().cap.am.max_bcopy, 0, *sender);
        sender->connect(0, *receiver, i);
        senders.push_back(sender);
        buffers.push_back(buffer);
    }

    m_am_count = 0;

    status = uct_iface_set_am_handler(receiver->iface(), AM_ID, am_handler,
                                      (void*)this, UCT_AM_CB_FLAG_SYNC);
    ASSERT_UCS_OK(status);

    for (unsigned i = 0; i < num_sends; ++i) {
        unsigned sender_num = rand() % NUM_SENDERS;

        mapped_buffer& buffer = buffers.at(sender_num);
        buffer.pattern_fill(i);

        ssize_t packed_len;
        for (;;) {
            entity& sender = senders.at(sender_num);
            packed_len = uct_ep_am_bcopy(sender.ep(0), AM_ID, mapped_buffer::pack,
                                         (void*)&buffer);
            if (packed_len != UCS_ERR_NO_RESOURCE) {
                break;
            }
            sender.progress();
            receiver->progress();
        }
        if (packed_len < 0) {
            ASSERT_UCS_OK((ucs_status_t)packed_len);
        }
    }

    while (m_am_count < num_sends) {
        receiver->progress();
    }

    status = uct_iface_set_am_handler(receiver->iface(), AM_ID, NULL, NULL,
                                      UCT_AM_CB_FLAG_SYNC);
    ASSERT_UCS_OK(status);

    check_backlog();

    for (unsigned i = 0; i < NUM_SENDERS; ++i) {
        senders.at(i).flush();
    }

    buffers.clear();
    senders.clear();
}

UCT_INSTANTIATE_NO_SELF_TEST_CASE(test_many2one_am)
