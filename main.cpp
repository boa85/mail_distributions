#include <thread>
#include <iostream>
#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "core/database/pg_connection.hpp"

using namespace md::db;
using namespace md::smtp;
void testConnection(const std::shared_ptr<PGBackend> &pgBackend)
{
    auto conn = pgBackend->connection();

    std::string demo = "SELECT max(id) FROM demo; ";
    PQsendQuery(conn->connection().get(), demo.c_str());

    while (auto res_ = PQgetResult(conn->connection().get())) {
        if (PQresultStatus(res_) == PGRES_TUPLES_OK && PQntuples(res_)) {
            auto ID = PQgetvalue(res_, 0, 0);
            std::cout << ID << std::endl;
        }

        if (PQresultStatus(res_) == PGRES_FATAL_ERROR) {
            std::cout << PQresultErrorMessage(res_) << std::endl;
        }

        PQclear(res_);
    }

    pgBackend->free_connection(conn);

}

void sendMail()
{
    bool bError = false;

    try
    {
        CSmtp mail;

        mail.set_smtp_server("smtp.domain.com", 25);
        mail.set_login("***");
        mail.set_password("***");
        mail.set_sender_name("User");
        mail.set_sender_mail("user@domain.com");
        mail.set_reply_to("user@domain.com");
        mail.set_subject(("The message"));
        mail.add_recipient("user@user.com", "user");
        mail.set_xpriority(XPRIORITY_NORMAL);
        mail.set_xmailer("The Bat! (v3.02) Professional");
        mail.add_msg_line("Hello,");
        mail.add_msg_line("");
        mail.add_msg_line("...");
        mail.add_msg_line("How are you today?");
        mail.add_msg_line("");
        mail.add_msg_line("Regards");
//        mail.modMsgLine(5, "regards");
//        mail.delMsgLine(2);
//        mail.addMsgLine("User");

        //mail.addAttachment("../test1.jpg");
        //mail.addAttachment("c:\\test2.exe");
        //mail.addAttachment("c:\\test3.txt");
        mail.send_mail();
    }
    catch(ECSmtp e)
    {
        std::cout << "Error: " << e.get_error_message().c_str() << ".\n";
        bError = true;
    }
    if(!bError)
        std::cout << "m_mail was send_mail successfully.\n";

}

int main(int argc, char const *argv[])
{
    try {
        /*auto pgbackend = std::make_shared<PGBackend>();


        std::vector<std::shared_ptr<std::thread>> vec;

        for (size_t i = 0; i < 50; ++i) {

            vec.push_back(std::make_shared<std::thread>(std::thread(testConnection, pgbackend)));
        }

        for (auto &i : vec) {
            i.get()->join();
        }*/

        sendMail();

        return 0;
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
