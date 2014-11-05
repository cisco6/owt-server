/*
 * Copyright 2014 Intel Corporation All Rights Reserved. 
 * 
 * The source code contained or described herein and all documents related to the 
 * source code ("Material") are owned by Intel Corporation or its suppliers or 
 * licensors. Title to the Material remains with Intel Corporation or its suppliers 
 * and licensors. The Material contains trade secrets and proprietary and 
 * confidential information of Intel or its suppliers and licensors. The Material 
 * is protected by worldwide copyright and trade secret laws and treaty provisions. 
 * No part of the Material may be used, copied, reproduced, modified, published, 
 * uploaded, posted, transmitted, distributed, or disclosed in any way without 
 * Intel's prior express written permission.
 * 
 * No license under any patent, copyright, trade secret or other intellectual 
 * property right is granted to or conferred upon you by disclosure or delivery of 
 * the Materials, either expressly, by implication, inducement, estoppel or 
 * otherwise. Any license under such intellectual property rights must be express 
 * and approved by Intel in writing.
 */

#include "WebRTCGateway.h"

using namespace erizo;

namespace mcu {

DEFINE_LOGGER(WebRTCGateway, "mcu.WebRTCGateway");

WebRTCGateway::WebRTCGateway()
    : m_feedback(nullptr)
    , m_mixer(nullptr)
{
}

WebRTCGateway::~WebRTCGateway()
{
    closeAll();
}

int WebRTCGateway::deliverAudioData(char* buf, int len, MediaSource* from)
{
    if (len <= 0)
        return 0;

    std::map<std::string, boost::shared_ptr<MediaSink>>::iterator it;
    boost::shared_lock<boost::shared_mutex> lock(m_subscriberMutex);
    for (it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
        if ((*it).second)
            (*it).second->deliverAudioData(buf, len);
    }
    lock.unlock();

    if (m_mixer && m_mixer->mediaSink())
        m_mixer->mediaSink()->deliverAudioData(buf, len, from);

    return len;
}

int WebRTCGateway::deliverVideoData(char* buf, int len, MediaSource* from)
{
    if (len <= 0)
        return 0;

    std::map<std::string, boost::shared_ptr<MediaSink>>::iterator it;
    boost::shared_lock<boost::shared_mutex> lock(m_subscriberMutex);
    for (it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
        if ((*it).second)
            (*it).second->deliverVideoData(buf, len);
    }
    lock.unlock();

    if (m_mixer && m_mixer->mediaSink())
        m_mixer->mediaSink()->deliverVideoData(buf, len, from);

    return len;
}

bool WebRTCGateway::setPublisher(MediaSource* publisher)
{
    publisher->setAudioSink(this);
    publisher->setVideoSink(this);

    m_publisher.reset(publisher);
    m_feedback = m_publisher->getFeedbackSink();

    // Automatically put the publisher as a source of the mixer.
    if (m_mixer)
        m_mixer->addSource(publisher);

    return true;
}

void WebRTCGateway::unsetPublisher()
{
    if (m_mixer)
        m_mixer->removeSource(m_publisher.get());

    m_feedback = nullptr;
    m_publisher.reset();
}

void WebRTCGateway::addSubscriber(MediaSink* subscriber, const std::string& id)
{
    ELOG_DEBUG("Adding subscriber");
    ELOG_DEBUG("From %u, %u ", m_publisher->getAudioSourceSSRC(), m_publisher->getVideoSourceSSRC());

    subscriber->setAudioSinkSSRC(m_publisher->getAudioSourceSSRC());
    subscriber->setVideoSinkSSRC(m_publisher->getVideoSourceSSRC());

    FeedbackSource* fbsource = subscriber->getFeedbackSource();
    if (fbsource) {
        ELOG_DEBUG("adding fbsource");
        fbsource->setFeedbackSink(m_feedback);
    }

    boost::unique_lock<boost::shared_mutex> lock(m_subscriberMutex);
    m_subscribers[id] = boost::shared_ptr<MediaSink>(subscriber);
}

void WebRTCGateway::removeSubscriber(const std::string& id)
{
    ELOG_DEBUG("removing subscriber: id is %s", id.c_str());
    boost::unique_lock<boost::shared_mutex> lock(m_subscriberMutex);
    std::map<std::string, boost::shared_ptr<MediaSink>>::iterator it = m_subscribers.find(id);
    if (it != m_subscribers.end())
        m_subscribers.erase(it);
}

void WebRTCGateway::setAdditionalSourceConsumer(woogeen_base::MediaSourceConsumer* mixer)
{
    m_mixer = mixer;
    if (m_publisher)
        mixer->addSource(m_publisher.get());
}

void WebRTCGateway::closeAll()
{
    ELOG_DEBUG ("WebRTCGateway closeAll");

    unsetPublisher();

    boost::unique_lock<boost::shared_mutex> subscriberLock(m_subscriberMutex);
    std::map<std::string, boost::shared_ptr<MediaSink>>::iterator subscriberItor = m_subscribers.begin();
    while (subscriberItor != m_subscribers.end()) {
        boost::shared_ptr<MediaSink>& subscriber = subscriberItor->second;
        if (subscriber) {
            FeedbackSource* fbsource = subscriber->getFeedbackSource();
            if (fbsource)
                fbsource->setFeedbackSink(nullptr);
        }
        m_subscribers.erase(subscriberItor++);
    }
    m_subscribers.clear();

    m_mixer = nullptr;
}

}/* namespace mcu */