/*
 * IdleEvent.h
 *
 *  Created on: Dec 14, 2025
 *      Author: samay099
 */

#ifndef SRC_IDLEEVENT_H_
#define SRC_IDLEEVENT_H_

class IdleEvent {
public:
	IdleEvent();
	virtual ~IdleEvent();
	void vHandleEvent() override;
};

#endif /* SRC_IDLEEVENT_H_ */
