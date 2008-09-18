#define PW_MIN_LENGTH   4
#define APP_PASS_LEN	1024

typedef struct pw_cb_data
{
    const char *password;
	const char *prompt_info;
} PW_CB_DATA;

int password_callback(char *buf, int bufsiz, int verify,
                      PW_CB_DATA *cb_tmp);

int app_passwd(BIO *err, char *arg1, char *arg2, char **pass1, char **pass2);

