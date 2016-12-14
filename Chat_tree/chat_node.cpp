#include "opt.h"

#define SUCCESS 0
#define FAIL -1

bool root = true;
bool terminating_state = false;
int service_info_offset = SIO; /*Зарезервированное место в пакете для типа сообщения и id*/

std::vector<node> neighbors;
std::vector<message> msgs;

int queue_overload_processing();

void terminating(int sig) {
    fprintf(stderr, "\nI'am dying...\n");
    terminating_state = true;
}

uint create_conReq_msg() {

    message new_msg;

    new_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (new_msg.package == NULL) {
        perror("calloc() failed");
        return 0;
    }

    new_msg.id = rand() / MSG_ID_RANGE + 1;
    new_msg.send_counter = 1;

    new_msg.package[0] = CONNECT_REQUEST;
    memcpy((new_msg.package + 1), &new_msg.id, sizeof (new_msg.id));

    /*Если очередь переполнена по возможности пытаемся освободить место*/
    queue_overload_processing();

    msgs.push_back(new_msg);

    return new_msg.id;
}

uint create_connectTo_msg(int ip, int port) { //Сообщение-команда переключиться на другого родителя

    message new_msg;

    new_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (new_msg.package == NULL) {
        perror("calloc() failed");
        return 0;
    }

    new_msg.id = rand() / MSG_ID_RANGE + 1;
    new_msg.send_counter = neighbors.size() - 1;

    new_msg.package[0] = CONNECT_TO;
    memcpy((new_msg.package + 1), &new_msg.id, sizeof (new_msg.id));
    memcpy((new_msg.package + service_info_offset), &ip, sizeof (ip));
    memcpy((new_msg.package + service_info_offset + sizeof (ip)), &port, sizeof (port));

    /*Если очередь переполнена по возможности пытаемся освободить место*/
    queue_overload_processing();

    msgs.push_back(new_msg);

    return new_msg.id;
}

/*@new_root - if reciever should become a new root*/
uint create_disconnect_msg(bool new_root) {

    message new_msg;

    new_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (new_msg.package == NULL) {
        perror("calloc() failed");
        return 0;
    }

    new_msg.id = rand() / MSG_ID_RANGE + 1;
    new_msg.send_counter = 1;

    new_msg.package[0] = DISCONNECT;
    memcpy((new_msg.package + 1), &new_msg.id, sizeof (new_msg.id));
    memcpy((new_msg.package + service_info_offset), &new_root, sizeof (new_root));

    /*Если очередь переполнена по возможности пытаемся освободить место*/
    queue_overload_processing();

    msgs.push_back(new_msg);

    return new_msg.id;
}

uint create_ack(uint msg_id) {

    message new_msg;

    new_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (new_msg.package == NULL) {
        perror("calloc() failed");
        return 0;
    }

    new_msg.id = rand() / MSG_ID_RANGE + 1;
    new_msg.send_counter = 1;

    new_msg.package[0] = ACK;
    memcpy((new_msg.package + 1), &msg_id, sizeof (new_msg.id));

    /*Если очередь переполнена по возможности пытаемся освободить место*/
    queue_overload_processing();

    msgs.push_back(new_msg);

    return new_msg.id;
}

uint create_msg(char * content) {

    message new_msg;

    new_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (new_msg.package == NULL) {
        perror("calloc() failed");
        return 0;
    }

    new_msg.id = rand() / MSG_ID_RANGE + 1;
    new_msg.send_counter = neighbors.size();

    new_msg.package[0] = MSG;
    memcpy((new_msg.package + 1), &new_msg.id, sizeof (new_msg.id));
    memcpy((new_msg.package + service_info_offset), content, (MSG_SIZE - service_info_offset));

    /*Если очередь переполнена по возможности пытаемся освободить место*/
    if (queue_overload_processing() == SUCCESS) {
        msgs.push_back(new_msg);
        return new_msg.id;
    } else {
        free(new_msg.package);
        return 0;
    }
}

node create_node(int ip, int port, bool is_parent) {

    node new_node;

    new_node.addr.sin_family = AF_INET;
    new_node.addr.sin_port = port;
    new_node.addr.sin_addr.s_addr = ip;
    new_node.is_parent = is_parent;
    new_node.death_counter = 0;

    return new_node;
}

void remove_neighbor(int idx) {
    /*Декрементим счетчики отправки у сообщений из msgs, которые надо было отправить отвалившейся ноде*/
    for (int i = 0; i < neighbors[idx].msgs_to_send.size(); i++)
        for (int j = 0; j < msgs.size(); j++)
            if (neighbors[idx].msgs_to_send[i].msg_id == msgs[j].id) {
                msgs[j].send_counter--;
            }

    neighbors.erase(neighbors.begin() + idx);
}

int main(int argc, char * argv[]) {
    char *nickname = NULL;
    char nicklen;
    int sock;
    int ret, percent_of_losses;
    struct sockaddr_in my_addr, addr;
    socklen_t addrlen = sizeof (addr);

    srand(time(NULL));

    if ((argc != 4) && (argc != 6)) {
        fprintf(stderr, "Usage %s <port> <nickname> <percentage of losses> [parent ip, parent port]\n", argv[0]);
        fprintf(stderr, "<nickname> -- less than 8 symbols\n");
        exit(0);
    }

    nicklen = strlen(argv[2]);
    if (nicklen > 8) nicklen = 8;

    nickname = (char *) calloc(sizeof (char), nicklen + 1);
    if (nickname == NULL) {
        perror("сalloc() failed");
        exit(0);
    }

    memcpy(nickname, argv[2], nicklen);

    percent_of_losses = atoi(argv[3]);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket() failed");
        exit(0);
    }

    memset((char *) &my_addr, 0, sizeof (my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(argv[1]));
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(sock, (struct sockaddr*) &my_addr, sizeof (my_addr));
    if (ret < 0) {
        perror("bind() failed");
        exit(0);
    }

    if (argc == 6) {
        node new_nod = create_node(inet_addr(argv[4]), htons(atoi(argv[5])), false); //parent node	
        send_info midala;
        midala.msg_id = create_conReq_msg();
        midala.attempt = DEFAULT_ATTEMPT_NUMB;
        midala.last_attempt = 0;
        if (midala.msg_id) new_nod.msgs_to_send.push_back(midala);
        neighbors.push_back(new_nod);
    }

    struct timeval timeout;
    fd_set read_fds;

    message incoming_msg;

    incoming_msg.package = (char *) calloc(sizeof (char), MSG_SIZE);
    if (incoming_msg.package == NULL) {
        perror("calloc() failed");
        exit(0);
    }

    char * outgoing_msg = (char *) calloc(sizeof (char), (MSG_SIZE - service_info_offset));
    if (outgoing_msg == NULL) {
        perror("calloc() failed");
        exit(0);
    }
    memcpy(outgoing_msg, nickname, nicklen + 1);
    free(nickname);

    fprintf(stderr, "           --Chat session started--\n");
    fprintf(stderr, "Port: %d\nRoot: %d\n", my_addr.sin_port, root);
    fprintf(stderr, "Nickname: %s\n\n", outgoing_msg);

    outgoing_msg[nicklen] = ':';
    uint out_msg_prefix_len = strlen(outgoing_msg);

    bool prepare_to_correct_terminating = false;

    signal(SIGINT, terminating);

    while (true) {

        memset((char *) &addr, 0, sizeof (addr));

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        if (!terminating_state) {
            FD_SET(0, &read_fds);
        }
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ret = select((sock + 1), &read_fds, NULL, NULL, &timeout);

        if (ret < 0)
            if (errno != EINTR) {
                perror("select() failed");
                break;
            } else {
                /* SIGINT пришел во время селекта */
                continue;
            }


        /*Соседей нет и пришел сигнал SIGINT*/
        if ((neighbors.size() == 0) && terminating_state) {
            break;
        }


        /*Есть соседи и пришел сигнал SIGINT - обрабатываем корректное отсоединение*/
        if (neighbors.size() > 0 && terminating_state && !prepare_to_correct_terminating) {

            prepare_to_correct_terminating = true;
            send_info sinfo;
            sinfo.msg_id = 0;
            sinfo.attempt = DEFAULT_ATTEMPT_NUMB;
            sinfo.last_attempt = 0;

            /*Если мы root*/
            if (root) {
                /*Выбираем первого ребенка в списке в качестве нового рута и говорим всем остальным детям присоединиться к нему*/
                sinfo.msg_id = create_connectTo_msg(neighbors[0].addr.sin_addr.s_addr, neighbors[0].addr.sin_port);

                if (sinfo.msg_id) {
                    for (int i = 1; i < neighbors.size(); i++) {
                        neighbors[i].msgs_to_send.push_back(sinfo);
                    }
                }

                /*Для нового рута оформляем дисконект*/
                sinfo.msg_id = create_disconnect_msg(true);
                if (sinfo.msg_id)
                    neighbors[0].msgs_to_send.push_back(sinfo);

            } else {
                /*Мы не рут - обходим всех соседей*/

                /*Находим родителя и формируем сообщения на дисконект с родителем и переконект детей*/
                for (int i = 0; i < neighbors.size(); i++) {
                    if (neighbors[i].is_parent) {
                        sinfo.msg_id = create_disconnect_msg(false);
                        if (sinfo.msg_id) neighbors[i].msgs_to_send.push_back(sinfo);
                        sinfo.msg_id = create_connectTo_msg(neighbors[i].addr.sin_addr.s_addr, neighbors[i].addr.sin_port);

                        break;
                    }
                }

                /*Всем кроме родителя в очередь добавляем id сообщения на переконект*/
                if (sinfo.msg_id) {
                    for (int i = 0; i < neighbors.size(); i++) {
                        if (!neighbors[i].is_parent) {
                            neighbors[i].msgs_to_send.push_back(sinfo);
                        }
                    }
                }
            }
        }


        /***********************/
        /*   Сработал select   */
        /***********************/

        if (ret > 0) {


            /**********************/
            /* Приняли датаграмму */
            /**********************/

            if (FD_ISSET(sock, &read_fds)) {

                for (int i = 0; i < 1; i++) { /*Небольшой костыль...*/

                    if (incoming_msg.package) {

                        addrlen = sizeof (addr);

                        ret = recvfrom(sock, incoming_msg.package, MSG_SIZE, 0, (sockaddr *) & addr, &addrlen);

                        if (ret < 0) {
                            perror("recvfrom() failed");
                            break;
                        }

                        int attempt_to_get_msg = rand() % 100;
                        //fprintf(stderr, "percent_of_losses = %d, rand = %d\n", percent_of_losses, attempt_to_get_msg);

                        /*Проверка "потери" датаграммы*/
                        if (attempt_to_get_msg >= percent_of_losses) {
                            int sender_idx = -1;

                            /*Ищем, кто из соседей отправил нам сообщение*/
                            for (int i = 0; i < neighbors.size(); i++) {
                                if ((addr.sin_port == neighbors[i].addr.sin_port) && (addr.sin_addr.s_addr == neighbors[i].addr.sin_addr.s_addr)) {
                                    /*Сбрасываем счетчик смерти и запоминаем номер отправителя*/
                                    neighbors[i].death_counter = 0;
                                    sender_idx = i;
                                    break;
                                }
                            }

                            /*Смотрим тип полученного сообщения*/
                            switch (incoming_msg.package[0]) {

                                    /*Запрос на коннект*/
                                case CONNECT_REQUEST:
                                {
                                    /*Если запрос пришел тогда, когда мы уже завершаем работу - не обрабатываем*/
                                    if (terminating_state) break;

                                    //fprintf(stderr, "CONNECT_REQUEST from %d\n", addr.sin_port);

                                    uint id;
                                    memcpy(&id, incoming_msg.package + 1, sizeof (id));

                                    /*Нода, желающая подключиться порой успевает послать больше одного запроса на подключение*/
                                    if (sender_idx != -1) {
                                        send_info sinfo;

                                        sinfo.msg_id = create_ack(id);
                                        sinfo.attempt = 1;
                                        sinfo.last_attempt = 0;
                                        if (sinfo.msg_id) neighbors[sender_idx].msgs_to_send.push_back(sinfo);

                                        break;
                                    }

                                    node neighbor = create_node(addr.sin_addr.s_addr, addr.sin_port, false);
                                    send_info sinfo;

                                    sinfo.msg_id = create_ack(id);
                                    sinfo.attempt = 1;
                                    sinfo.last_attempt = 0;
                                    if (sinfo.msg_id) neighbor.msgs_to_send.push_back(sinfo);

                                    neighbors.push_back(neighbor);
                                    break;
                                }
                                case CONNECT_TO:
                                {
                                    /*Если отправителя нет в списке соседей или мы уже завершаем сессию - игнорируем*/
                                    if ((sender_idx == -1) || terminating_state) break;

                                    uint id;
                                    memcpy(&id, incoming_msg.package + 1, sizeof (id));

                                    uint ack_id = create_ack(id);
                                    if (!ack_id) break;

                                    /*Получилось создать ack сообщение - отправляем его в ответ (оно теперь последнее в векторе сообщений)*/
                                    sendto(sock, msgs[msgs.size() - 1].package, MSG_SIZE, 0, (struct sockaddr *) &neighbors[sender_idx].addr, sizeof (neighbors[sender_idx].addr));

                                    /*Сразу удаляеем этот ack*/
                                    free(msgs[msgs.size() - 1].package);
                                    msgs.erase(msgs.end() - 1);

                                    /*Производим "деинсталяцию" соседа*/
                                    remove_neighbor(sender_idx);

                                    int ip, port;

                                    memcpy(&ip, (incoming_msg.package + service_info_offset), sizeof (ip));
                                    memcpy(&port, (incoming_msg.package + service_info_offset + sizeof (ip)), sizeof (port));

                                    //fprintf(stderr, "CONNECT_TO from %d\n", port);

                                    node new_node = create_node(ip, port, false);
                                    root = true;

                                    send_info sinfo;
                                    sinfo.attempt = DEFAULT_ATTEMPT_NUMB;
                                    sinfo.msg_id = create_conReq_msg();
                                    sinfo.last_attempt = 0;

                                    if (sinfo.msg_id) new_node.msgs_to_send.push_back(sinfo);
                                    neighbors.push_back(new_node);

                                    break;
                                }
                                case ACK:
                                {
                                    /*Непонятно от кого подтверждения не принимаем*/
                                    if (sender_idx == -1) break;

                                    //fprintf(stderr, "ACK from %d\n", addr.sin_port);

                                    uint id;
                                    memcpy(&id, (incoming_msg.package + 1), sizeof (id));

                                    /*Смотрим, на что нам пришло подтверждение*/
                                    for (int i = 0; i < msgs.size(); i++) {
                                        if (msgs[i].id == id) {

                                            /*Если подтвердился запрос на реконнект потомка к родителю или дисконект, удаляем ноду из списка соседей*/
                                            if ((msgs[i].package[0] == CONNECT_TO) || (msgs[i].package[0] == DISCONNECT)) {
                                                remove_neighbor(sender_idx);
                                                break;
                                            }

                                            /*Если подтвердился коннект к ноде, помечаем ее как родителя и снимаем флаг рута*/
                                            if (msgs[i].package[0] == CONNECT_REQUEST) {
                                                neighbors[sender_idx].is_parent = true;
                                                fprintf(stderr,"Parent node confirmed the connection\nThe root flag is removed\n");
                                                root = false;
                                            }

                                            /*Уменьшаем счетчик отправки сообщения (скольким нодам нужно переслать сообщение)*/
                                            msgs[i].send_counter--;

                                            /*Удаляем сообщение (id) из очереди на отправку у sendera*/
                                            for (int i = 0; i < neighbors[sender_idx].msgs_to_send.size(); i++) {
                                                if (neighbors[sender_idx].msgs_to_send[i].msg_id == id) {
                                                    neighbors[sender_idx].msgs_to_send.erase(neighbors[sender_idx].msgs_to_send.begin() + i);
                                                    break;
                                                }
                                            }

                                            break;
                                        }
                                    }

                                    break;
                                }
                                case MSG:
                                {
                                    if ((sender_idx == -1) || terminating_state) break;

                                    fprintf(stderr, "%s\n", (incoming_msg.package + service_info_offset));

                                    message to_resend;
                                    to_resend.package = (char *) calloc(sizeof (char), MSG_SIZE);
                                    if (to_resend.package != NULL) {

                                        memcpy(to_resend.package, incoming_msg.package, MSG_SIZE);

                                        memcpy(&to_resend.id, (incoming_msg.package + 1), sizeof (to_resend.id));
                                        to_resend.send_counter = neighbors.size() - 1;
                                        msgs.push_back(to_resend);

                                        /*Добавляем пришедший msg в очередь на отправку всем, кроме его отправителя (ему посылаем ack)*/
                                        for (int i = 0; i < neighbors.size(); i++) {
                                            if (i == sender_idx) {
                                                send_info sinfo;
                                                sinfo.msg_id = create_ack(to_resend.id);
                                                sinfo.attempt = 1;
                                                sinfo.last_attempt = 0;
                                                if (sinfo.msg_id) neighbors[i].msgs_to_send.push_back(sinfo);
                                            } else {
                                                send_info sinfo;
                                                sinfo.msg_id = to_resend.id;
                                                sinfo.attempt = DEFAULT_ATTEMPT_NUMB;
                                                sinfo.last_attempt = 0;
                                                neighbors[i].msgs_to_send.push_back(sinfo);
                                            }
                                        }
                                    } else {
                                        perror("calloc() failed");
                                    }

                                    break;
                                }
                                case DISCONNECT:
                                {
                                    if (sender_idx == -1) break;
                                    //fprintf(stderr, "DISCONNECT from %d\n", addr.sin_port);

                                    uint id;
                                    memcpy(&id, incoming_msg.package + 1, sizeof (id));

                                    uint ack_id = create_ack(id);
                                    if (!ack_id) break;

                                    /*Получилось создать ack сообщение - отправляем его в ответ (оно теперь последнее в векторе сообщений)*/
                                    sendto(sock, msgs[msgs.size() - 1].package, MSG_SIZE, 0, (struct sockaddr *) &neighbors[sender_idx].addr, sizeof (neighbors[sender_idx].addr));

                                    /*Сразу удаляеем этот ack*/
                                    free(msgs[msgs.size() - 1].package);
                                    msgs.erase(msgs.end() - 1);

                                    bool i_am_new_root;
                                    memcpy(&i_am_new_root, (incoming_msg.package + service_info_offset), sizeof (i_am_new_root));
                                    if (i_am_new_root)
                                        root = true;

                                    remove_neighbor(sender_idx);

                                    break;
                                }
                            }
                        }
                    }
                }
            }

            /********************************/
            /* Ввели сообщение с клавиатуры */
            /********************************/

            if (FD_ISSET(0, &read_fds)) {

                int ret = read(0, (outgoing_msg + out_msg_prefix_len), (MSG_SIZE - service_info_offset - out_msg_prefix_len));

                if (ret < 0) {
                    perror("read() failed");
                }

                if (ret > 0) {
                    outgoing_msg[out_msg_prefix_len + ret] = '\0';

                    send_info sinfo;
                    sinfo.msg_id = create_msg(outgoing_msg);
                    sinfo.attempt = UNSENT_MSGS_LIMIT;
                    sinfo.last_attempt = 0;

                    /*Добавляем всем соседям в очередь на отправку*/
                    if (sinfo.msg_id) {
                        for (int i = 0; i < neighbors.size(); i++) {
                            neighbors[i].msgs_to_send.push_back(sinfo);
                        }
                    }
                }
            }
        }



        /*********************************/
        /* Отправка сообщений из очереди */
        /*********************************/

        bool step_out = false;

        /*Проверяем очередь сообщений*/
        for (int i = 0; i < msgs.size(); i++) {
            /*Если сообщение разослали всем, кому было нужно, удаляем его из очереди и продолжаем*/
            if (msgs[i].send_counter <= 0) {
                free(msgs[i].package);
                msgs.erase(msgs.begin() + i);
                i--;
                continue;
            }

            /*Бежим по всем соседям...*/
            for (int j = 0; j < neighbors.size(); j++) {
                /*и по всем их сообщениям*/
                for (int k = 0; k < neighbors[j].msgs_to_send.size(); k++) {

                    /*Если в очереди соседа нашелся id из общей очереди сообщений и прошло определенное количество времени с последней попытки отправки, то переотправляем*/
                    if ((msgs[i].id == neighbors[j].msgs_to_send[k].msg_id) && ((time(NULL) - neighbors[j].msgs_to_send[k].last_attempt) > TIMEOUT)) {

                        addrlen = sizeof (neighbors[j].addr);
                        ssize_t ret = sendto(sock, msgs[i].package, MSG_SIZE, 0, (struct sockaddr *) &(neighbors[j].addr), addrlen);
                        if (ret != MSG_SIZE) continue;

                        /*Если мы отправили */
                        if (msgs[i].package[0] == ACK) {
                            neighbors[j].msgs_to_send.erase(neighbors[j].msgs_to_send.begin() + k);

                            free(msgs[i].package);
                            msgs.erase(msgs.begin() + i);
                            i--;

                            /*Необходимо выйти до внешнего цикла*/
                            step_out = true;
                            break;
                        }

                        neighbors[j].msgs_to_send[k].attempt--;
                        neighbors[j].msgs_to_send[k].last_attempt = time(NULL);

                        if (neighbors[j].msgs_to_send[k].attempt == 0) {

                            if ((msgs[i].package[0] == CONNECT_TO) || (msgs[i].package[0] == DISCONNECT)) {
                                /*Это необходимо, чтобы сразу исключить детей и родителя из списка соседей после отпущенного числа попыток*/
                                neighbors[j].death_counter = UNSENT_MSGS_LIMIT;
                            }

                            msgs[i].send_counter--;
                            if (msgs[i].send_counter <= 0) {
                                free(msgs[i].package);
                                msgs.erase(msgs.begin() + i);
                                i--;
                                step_out = true;
                            }

                            /*Увеличиваем счетчик смерти - число неподтвержденных сообщений*/
                            neighbors[j].death_counter++;
                            /*Превышен лимит - нода удаляется из списка соседей*/
                            if (neighbors[j].death_counter >= UNSENT_MSGS_LIMIT) {
                                remove_neighbor(j);
                                j--;
                                break;
                            } else {
                                neighbors[j].msgs_to_send.erase(neighbors[j].msgs_to_send.begin() + k);
                                k--;
                            }
                        }
                    }
                }
                if (step_out) break;
            }
        }




    }

    free(incoming_msg.package);
    free(outgoing_msg);

    return 0;
}




/*********************************************/
/* Обработка переполнениия очереди сообщений */

/*********************************************/

int queue_overload_processing() {

    for (int i = 0; i < msgs.size(); i++) {

        /*Если в пределах нормы - выходим из цикла*/
        if (msgs.size() < MSG_QUEUE_MAX_SIZE) return SUCCESS;

        /*Если не сервисное сообщение можем удалить*/
        if (msgs[i].package[0] == MSG) {

            for (int j = 0; j < neighbors.size(); j++) {
                for (int k = 0; k < neighbors[j].msgs_to_send.size(); k++) {
                    /*Если в очереди соседа нашелся id первого сообщения из общей очереди сообщений - убираем его*/
                    if ((msgs[i].id == neighbors[j].msgs_to_send[k].msg_id)) {
                        neighbors[j].msgs_to_send.erase(neighbors[j].msgs_to_send.begin() + k);
                        break;
                    }
                }
            }

            /*Удаляем это сообщение из очереди*/
            free(msgs[i].package);
            msgs.erase(msgs.begin());
        }
    }

    if (msgs.size() < MSG_QUEUE_MAX_SIZE) return SUCCESS;

    return FAIL;
}
