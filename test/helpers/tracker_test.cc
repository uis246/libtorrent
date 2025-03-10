#include "config.h"

#include "test/helpers/tracker_test.h"

#include "net/address_list.h"
#include "test/torrent/test_tracker_list.h"

#include <cppunit/extensions/HelperMacros.h>

// TrackerTest does not fully support threaded tracker yet, so review the code if such a requirement
// is needed.

uint32_t return_new_peers = 0xdeadbeef;

void
TrackerTest::set_success(uint32_t counter, uint32_t time_last) {
  auto guard = lock_guard();
  state().m_success_counter = counter;
  state().m_success_time_last = time_last;
  state().set_normal_interval(torrent::tracker::TrackerState::default_normal_interval);
  state().set_min_interval(torrent::tracker::TrackerState::default_min_interval);
}

void
TrackerTest::set_failed(uint32_t counter, uint32_t time_last) {
  auto guard = lock_guard();
  state().m_failed_counter = counter;
  state().m_failed_time_last = time_last;
  state().m_normal_interval = 0;
  state().m_min_interval = 0;
}

void
TrackerTest::set_latest_new_peers(uint32_t peers) {
  auto guard = lock_guard();
  state().m_latest_new_peers = peers;
}

void
TrackerTest::set_latest_sum_peers(uint32_t peers) {
  auto guard = lock_guard();
  state().m_latest_sum_peers = peers;
}

void
TrackerTest::set_new_normal_interval(uint32_t timeout) {
  auto guard = lock_guard();
  state().set_normal_interval(timeout);
}

void
TrackerTest::set_new_min_interval(uint32_t timeout) {
  auto guard = lock_guard();
  state().set_min_interval(timeout);
}

void
TrackerTest::send_event(torrent::tracker::TrackerState::event_enum new_state) {
  // Trackers close on-going requests when new state is sent.
  m_busy = true;
  m_open = true;
  m_requesting_state = new_state;
  lock_and_set_latest_event(new_state);
}

void
TrackerTest::send_scrape() {
  // We ignore scrapes if we're already making a request.
  // if (m_open)
  //   return;

  m_busy = true;
  m_open = true;
  m_requesting_state = torrent::tracker::TrackerState::EVENT_SCRAPE;

  lock_and_set_latest_event(torrent::tracker::TrackerState::EVENT_SCRAPE);
}

bool
TrackerTest::trigger_success(uint32_t new_peers, uint32_t sum_peers) {
  CPPUNIT_ASSERT(is_busy() && is_open());

  torrent::AddressList address_list;

  for (unsigned int i = 0; i != sum_peers; i++) {
    rak::socket_address sa; sa.sa_inet()->clear(); sa.sa_inet()->set_port(0x100 + i);
    address_list.push_back(sa);
  }

  return trigger_success(&address_list, new_peers);
}

bool
TrackerTest::trigger_success(torrent::AddressList* address_list, uint32_t new_peers) {
  CPPUNIT_ASSERT(is_busy() && is_open());

  m_busy = false;
  m_open = !(state().flags() & flag_close_on_done);
  return_new_peers = new_peers;

  if (state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE) {
    if (m_slot_scrape_success)
      m_slot_scrape_success();

  } else {
    {
      auto guard = lock_guard();
      state().set_normal_interval(torrent::tracker::TrackerState::default_normal_interval);
      state().set_min_interval(torrent::tracker::TrackerState::default_min_interval);
    }

    if (m_slot_success)
      m_slot_success(std::move(*address_list));
  }

  m_requesting_state = -1;
  return true;
}

bool
TrackerTest::trigger_failure() {
  CPPUNIT_ASSERT(is_busy() && is_open());

  m_busy = false;
  m_open = !(state().flags() & flag_close_on_done);
  return_new_peers = 0;

  if (state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE) {
    if (m_slot_scrape_failure)
      m_slot_scrape_failure("failed");

  } else {
    {
      auto guard = lock_guard();
      state().set_normal_interval(0);
      state().set_min_interval(0);
    }

    if (m_slot_failure)
      m_slot_failure("failed");
  }

  m_requesting_state = -1;
  return true;
}

bool
TrackerTest::trigger_scrape() {
  if (!is_busy() || !is_open())
    return false;

  if (state().latest_event() != torrent::tracker::TrackerState::EVENT_SCRAPE)
    return false;

  return trigger_success();
}
