#ifdef PQ_RM92AS_H
#define PQ_RM92AS_H

class RM92AS{
private:    // アクセス指定子
    Serial *_serial;
    char tx_buf[228];
    char rx_buf[228];
    char rx_size;
    int index;
    int flag;
    bool response;

    void (*func)(char*);

public:
    // Serialのインスタンスへの参照
    RM92AS(Serial &serial);

    void send(char *data, int size);

    void attach(void(*func_ptr)(char*));

private:
    void receive();
};

#endif