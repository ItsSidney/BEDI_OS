#ifndef RTC_H
#define RTC_H

typedef struct {
    int hour;
    int minute;
    int second;
} time_t;

void get_time(time_t* t);
void set_time_offset(int hours, int minutes);

#endif
