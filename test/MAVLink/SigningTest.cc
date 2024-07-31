/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SigningTest.h"
#include "MAVLinkSigning.h"
#include "QGCApplication.h"
#include "QGCToolbox.h"
#include "MultiVehicleManager.h"

#include <QtTest/QTest>
#include <QtTest/QSignalSpy>

void SigningTest::_testInitSigning()
{
    QVERIFY(MAVLinkSigning::initSigning(MAVLINK_COMM_0, "secret_key", nullptr));
    const mavlink_status_t *status = mavlink_get_channel_status(MAVLINK_COMM_0);
    const mavlink_signing_t *signing = status->signing;
    QVERIFY(signing);
    QVERIFY(signing->link_id == MAVLINK_COMM_0);
    QVERIFY(signing->flags & MAVLINK_SIGNING_FLAG_SIGN_OUTGOING);
    QVERIFY(memcmp(signing->secret_key, QCryptographicHash::hash("secret_key", QCryptographicHash::Sha256), sizeof(signing->secret_key)) == 0);
    QVERIFY(MAVLinkSigning::initSigning(MAVLINK_COMM_0, QByteArrayView("1", 32), nullptr));
    QVERIFY(memcmp(signing->secret_key, QCryptographicHash::hash(QByteArrayView("1", 32), QCryptographicHash::Sha256), sizeof(signing->secret_key)) == 0);
    QVERIFY(MAVLinkSigning::initSigning(MAVLINK_COMM_0, QByteArrayView(), nullptr));
    QVERIFY(!status->signing);
}

void SigningTest::_testCheckSigningLinkId()
{
    QVERIFY(MAVLinkSigning::initSigning(MAVLINK_COMM_0, "secret_key", nullptr));
    const mavlink_heartbeat_t heartbeat = {0};
    mavlink_message_t message;
    (void) mavlink_msg_heartbeat_encode_chan(1, MAV_COMP_ID_USER1, MAVLINK_COMM_0, &message, &heartbeat);
    QVERIFY(MAVLinkSigning::checkSigningLinkId(MAVLINK_COMM_0, message));
}

void SigningTest::_testCreateSetupSigning()
{
    QVERIFY(MAVLinkSigning::initSigning(MAVLINK_COMM_0, "secret_key", nullptr));
    const mavlink_system_t target_system = {1, MAV_COMP_ID_AUTOPILOT1};
    mavlink_setup_signing_t setup_signing;
    MAVLinkSigning::createSetupSigning(MAVLINK_COMM_0, target_system, setup_signing);
    QVERIFY(setup_signing.initial_timestamp != 0);
    QCOMPARE(setup_signing.target_system, target_system.sysid);
    QCOMPARE(setup_signing.target_component, target_system.compid);
}

void SigningTest::_denyUnsignedMessages()
{
    // Start a MockLink with signing enabled for incoming traffic. But QGC side of pipe is not setup for signing.
    Q_ASSERT(!_mockLink);
    _mockLink = MockLink::startMockLink(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR, false /* sendStatusText */, false /* isSecureConnection */, "SigningKey");
    QVERIFY(_mockLink);
    const mavlink_status_t *status = mavlink_get_channel_status(_mockLink->mavlinkChannel());
    const mavlink_signing_t *signing = status->signing;
    QVERIFY(!signing);

    // QGC will be sending unsigned messages to Vehicle. Those messsages will be not be accepted since they are not signed.
    QSignalSpy spyMockLink(_mockLink, SIGNAL(denyingUnsignedMessage()));
    QCOMPARE(spyMockLink.wait(1000), true);

    // Vehicle should still be created
    auto multiVehicleMgr = qgcApp()->toolbox()->multiVehicleManager();
    QSignalSpy spyVehicle(multiVehicleMgr, &MultiVehicleManager::activeVehicleChanged);
    QCOMPARE(spyVehicle.wait(5000), true);
    QCOMPARE(spyVehicle.count(), 1);
}
