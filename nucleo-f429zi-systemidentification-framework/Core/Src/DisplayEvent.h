/*
 * DisplayEvent.h
 *
 *  Created on: Dec 14, 2025
 *      Author: samay099
 */

#ifndef SRC_DISPLAYEVENT_H_
#define SRC_DISPLAYEVENT_H_

#include"Event.h"

class DisplayEvent : public Event{

public:
	DisplayEvent();
	virtual ~DisplayEvent();

	static DisplayEvent& getObject();

	void vHandleEvent() override;
};

#endif /* SRC_DISPLAYEVENT_H_ */
