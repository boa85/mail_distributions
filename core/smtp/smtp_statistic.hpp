//
// Created by boa on 12.04.19.
//

#ifndef MAIL_DISTRIBUTIONS_SMTP_STATISTIC_HPP
#define MAIL_DISTRIBUTIONS_SMTP_STATISTIC_HPP

struct SmtpStatistic
{
    int m_success_send_count = 0;
    int m_failed_send_count = 0;
    int m_total_send_count = 0;
    int m_number_read_messages = 0;
    int number_delivered_messages = 0;
    int number_open_links = 0;
};


#endif //MAIL_DISTRIBUTIONS_SMTP_STATISTIC_HPP
