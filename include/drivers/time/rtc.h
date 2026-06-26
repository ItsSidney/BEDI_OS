#ifndef RTC_H
#define RTC_H

typedef struct {
    int hour;
    int minute;
    int second;
    int day;
    int month;
    int year;
} time_t;

void get_time(time_t* t);
void set_time_offset(int hours, int minutes);
int is_updating(void);

#endif
