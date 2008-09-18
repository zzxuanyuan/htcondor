//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <ctype.h>
#include <openssl/err.h>
//#include <openssl/x509.h>
//#include <openssl/x509v3.h>
#include <openssl/pem.h>
//#include <openssl/pkcs12.h>
#include <openssl/ui.h>
//#include <openssl/safestack.h>

#include "ui_callback.h"

static UI_METHOD *ui_method = NULL;

static int ui_open(UI *ui)
	{
	return UI_method_get_opener(UI_OpenSSL())(ui);
	}
static int ui_read(UI *ui, UI_STRING *uis)
	{
	if (UI_get_input_flags(uis) & UI_INPUT_FLAG_DEFAULT_PWD
		&& UI_get0_user_data(ui))
		{
		switch(UI_get_string_type(uis))
			{
		case UIT_PROMPT:
		case UIT_VERIFY:
			{
			const char *password =
				((PW_CB_DATA *)UI_get0_user_data(ui))->password;
			if (password && password[0] != '\0')
				{
				UI_set_result(ui, uis, password);
				return 1;
				}
			}
		default:
			break;
			}
		}
	return UI_method_get_reader(UI_OpenSSL())(ui, uis);
	}
static int ui_write(UI *ui, UI_STRING *uis)
	{
	if (UI_get_input_flags(uis) & UI_INPUT_FLAG_DEFAULT_PWD
		&& UI_get0_user_data(ui))
		{
		switch(UI_get_string_type(uis))
			{
		case UIT_PROMPT:
		case UIT_VERIFY:
			{
			const char *password =
				((PW_CB_DATA *)UI_get0_user_data(ui))->password;
			if (password && password[0] != '\0')
				return 1;
			}
		default:
			break;
			}
		}
	return UI_method_get_writer(UI_OpenSSL())(ui, uis);
	}
static int ui_close(UI *ui)
	{
	return UI_method_get_closer(UI_OpenSSL())(ui);
	}
int setup_ui_method(void)
	{
	ui_method = UI_create_method("OpenSSL application user interface");
	UI_method_set_opener(ui_method, ui_open);
	UI_method_set_reader(ui_method, ui_read);
	UI_method_set_writer(ui_method, ui_write);
	UI_method_set_closer(ui_method, ui_close);
	return 0;
	}
void destroy_ui_method(void)
	{
	if(ui_method)
		{
		UI_destroy_method(ui_method);
		ui_method = NULL;
		}
	}

int password_callback(char *buf, int bufsiz, int verify,
	PW_CB_DATA *cb_tmp)
	{
	UI *ui = NULL;
	int res = 0;
	const char *prompt_info = NULL;
	const char *password = NULL;
	PW_CB_DATA *cb_data = (PW_CB_DATA *)cb_tmp;

	if (cb_data)
		{
		if (cb_data->password)
			password = cb_data->password;
		if (cb_data->prompt_info)
			prompt_info = cb_data->prompt_info;
		}

	if (password)
		{
		res = strlen(password);
		if (res > bufsiz)
			res = bufsiz;
		memcpy(buf, password, res);
		return res;
		}

	ui = UI_new_method(ui_method);
	if (ui)
		{
		int ok = 0;
		char *buff = NULL;
		int ui_flags = 0;
		char *prompt = NULL;

		prompt = UI_construct_prompt(ui, "pass phrase",
			prompt_info);

		ui_flags |= UI_INPUT_FLAG_DEFAULT_PWD;
		UI_ctrl(ui, UI_CTRL_PRINT_ERRORS, 1, 0, 0);

		if (ok >= 0)
			ok = UI_add_input_string(ui,prompt,ui_flags,buf,
				PW_MIN_LENGTH,BUFSIZ-1);
		if (ok >= 0 && verify)
			{
			buff = (char *)OPENSSL_malloc(bufsiz);
			ok = UI_add_verify_string(ui,prompt,ui_flags,buff,
				PW_MIN_LENGTH,BUFSIZ-1, buf);
			}
		if (ok >= 0)
			do
				{
				ok = UI_process(ui);
				}
			while (ok < 0 && UI_ctrl(ui, UI_CTRL_IS_REDOABLE, 0, 0, 0));

		if (buff)
			{
			OPENSSL_cleanse(buff,(unsigned int)bufsiz);
			OPENSSL_free(buff);
			}

		if (ok >= 0)
			res = strlen(buf);
		if (ok == -1)
			{
                //BIO_printf(bio_err, "User interface error\n");
                //ERR_print_errors(bio_err);
			OPENSSL_cleanse(buf,(unsigned int)bufsiz);
			res = 0;
			}
		if (ok == -2)
			{
                //BIO_printf(bio_err,"aborted!\n");
                //OPENSSL_cleanse(buf,(unsigned int)bufsiz);
			res = 0;
			}
		UI_free(ui);
		OPENSSL_free(prompt);
		}
	return res;
	}

static char *app_get_pass(BIO *err, char *arg, int keepbio);

int app_passwd(BIO *err, char *arg1, char *arg2, char **pass1, char **pass2)
{
	int same;
	if(!arg2 || !arg1 || strcmp(arg1, arg2)) same = 0;
	else same = 1;
	if(arg1) {
		*pass1 = app_get_pass(err, arg1, same);
		if(!*pass1) return 0;
	} else if(pass1) *pass1 = NULL;
	if(arg2) {
		*pass2 = app_get_pass(err, arg2, same ? 2 : 0);
		if(!*pass2) return 0;
	} else if(pass2) *pass2 = NULL;
	return 1;
}

static char *app_get_pass(BIO *err, char *arg, int keepbio)
{
	char *tmp, tpass[APP_PASS_LEN];
	static BIO *pwdbio = NULL;
	int i;
	if(!strncmp(arg, "pass:", 5)) return BUF_strdup(arg + 5);
	if(!strncmp(arg, "env:", 4)) {
		tmp = getenv(arg + 4);
		if(!tmp) {
			BIO_printf(err, "Can't read environment variable %s\n", arg + 4);
			return NULL;
		}
		return BUF_strdup(tmp);
	}
	if(!keepbio || !pwdbio) {
		if(!strncmp(arg, "file:", 5)) {
			pwdbio = BIO_new_file(arg + 5, "r");
			if(!pwdbio) {
				BIO_printf(err, "Can't open file %s\n", arg + 5);
				return NULL;
			}
		} else if(!strncmp(arg, "fd:", 3)) {
			BIO *btmp;
			i = atoi(arg + 3);
			if(i >= 0) pwdbio = BIO_new_fd(i, BIO_NOCLOSE);
			if((i < 0) || !pwdbio) {
				BIO_printf(err, "Can't access file descriptor %s\n", arg + 3);
				return NULL;
			}
			/* Can't do BIO_gets on an fd BIO so add a buffering BIO */
			btmp = BIO_new(BIO_f_buffer());
			pwdbio = BIO_push(btmp, pwdbio);
		} else if(!strcmp(arg, "stdin")) {
			pwdbio = BIO_new_fp(stdin, BIO_NOCLOSE);
			if(!pwdbio) {
				BIO_printf(err, "Can't open BIO for stdin\n");
				return NULL;
			}
		} else {
			BIO_printf(err, "Invalid password argument \"%s\"\n", arg);
			return NULL;
		}
	}
	i = BIO_gets(pwdbio, tpass, APP_PASS_LEN);
	if(keepbio != 1) {
		BIO_free_all(pwdbio);
		pwdbio = NULL;
	}
	if(i <= 0) {
		BIO_printf(err, "Error reading password from BIO\n");
		return NULL;
	}
	tmp = strchr(tpass, '\n');
	if(tmp) *tmp = 0;
	return BUF_strdup(tpass);
}
