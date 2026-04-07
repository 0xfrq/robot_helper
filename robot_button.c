#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>        /* _getch() untuk windows */
#include <windows.h>      /* Sleep() untuk windows  */
#include <libssh/libssh.h>

#define USERNAME  "robotis"
#define PASSWORD  "111111"
#define PORT      22
#define ROS_TOPIC "/robotis/open_cr/button"

/* peta nama tombol */
const char *nama_tombol[] = {
    NULL,          /* indeks 0 tidak dipakai */
    "mode",        /* 1 */
    "start",       /* 2 */
    "user",        /* 3 */
    "mode_long",   /* 4 */
    "start_long",  /* 5 */
    "user_long"    /* 6 */
};

/* ------------------------------------------------------------------ */
/* jalankan perintah di robot lewat SSH                                */
/* ------------------------------------------------------------------ */
int jalankan_perintah_ssh(ssh_session sesi, const char *perintah) {
    ssh_channel saluran = ssh_channel_new(sesi);
    if (!saluran) {
        fprintf(stderr, "gagal membuat saluran: %s\n", ssh_get_error(sesi));
        return -1;
    }

    if (ssh_channel_open_session(saluran) != SSH_OK) {
        fprintf(stderr, "gagal membuka sesi: %s\n", ssh_get_error(sesi));
        ssh_channel_free(saluran);
        return -1;
    }

    if (ssh_channel_request_exec(saluran, perintah) != SSH_OK) {
        fprintf(stderr, "gagal menjalankan perintah: %s\n", ssh_get_error(sesi));
        ssh_channel_close(saluran);
        ssh_channel_free(saluran);
        return -1;
    }

    /* baca output remote agar saluran selesai dengan bersih */
    char buf[256];
    int nbytes;
    while ((nbytes = ssh_channel_read(saluran, buf, sizeof(buf) - 1, 0)) > 0) {
        /* abaikan output */
    }

    ssh_channel_send_eof(saluran);
    ssh_channel_close(saluran);
    ssh_channel_free(saluran);
    return 0;
}

/* ------------------------------------------------------------------ */
/* publish tombol lalu berhenti setelah 1 detik                        */
/* ------------------------------------------------------------------ */
void publish_tombol(ssh_session sesi, const char *nama) {
    char perintah[512];

    /*
     * jalankan rostopic pub di background robot linux,
     * tunggu 1 detik, lalu kill prosesnya (simulasi ctrl+c)
     */
    snprintf(perintah, sizeof(perintah),
        "bash -l -c '"
        "source /opt/ros/kinetic/setup.bash 2>/dev/null; "
        "rostopic pub %s std_msgs/String \"{data: \\047%s\\047}\" &"
        " PID=$!; sleep 1; kill $PID 2>/dev/null'",
        ROS_TOPIC, nama);

    printf("publish: %s -> [%s] ... berhenti setelah 1 detik\n", ROS_TOPIC, nama);
    fflush(stdout);

    if (jalankan_perintah_ssh(sesi, perintah) != 0) {
        fprintf(stderr, "gagal publish tombol: %s\n", nama);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    /* ---- pilih robot --------------------------------------------- */
    printf("=== robotis ssh button publisher ===\n\n");
    printf("pilih robot (1-5):\n");
    printf("  1 -> 192.168.1.31\n");
    printf("  2 -> 192.168.1.32\n");
    printf("  3 -> 192.168.1.33\n");
    printf("  4 -> 192.168.1.34\n");
    printf("  5 -> 192.168.1.35\n");
    printf("\npilihan: ");
    fflush(stdout);

    /* gunakan _getch() agar langsung terbaca tanpa enter */
    char pilihan_char;
    do {
        pilihan_char = (char)_getch();
    } while (pilihan_char < '1' || pilihan_char > '5');

    int pilihan = pilihan_char - '0';
    printf("%d\n", pilihan);  /* tampilkan pilihan user */

    char host[32];
    snprintf(host, sizeof(host), "192.168.1.3%d", pilihan);
    printf("\nmenghubungkan ke %s sebagai %s ...\n", host, USERNAME);
    fflush(stdout);

    /* ---- koneksi SSH --------------------------------------------- */
    ssh_session sesi = ssh_new();
    if (!sesi) {
        fprintf(stderr, "gagal membuat sesi SSH.\n");
        return 1;
    }

    int port     = PORT;
    int noverify = 0;

    ssh_options_set(sesi, SSH_OPTIONS_HOST, host);
    ssh_options_set(sesi, SSH_OPTIONS_USER, USERNAME);
    ssh_options_set(sesi, SSH_OPTIONS_PORT, &port);
    ssh_options_set(sesi, SSH_OPTIONS_STRICTHOSTKEYCHECK, &noverify);

    if (ssh_connect(sesi) != SSH_OK) {
        fprintf(stderr, "koneksi gagal: %s\n", ssh_get_error(sesi));
        ssh_free(sesi);
        return 1;
    }

    if (ssh_userauth_password(sesi, NULL, PASSWORD) != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "autentikasi gagal: %s\n", ssh_get_error(sesi));
        ssh_disconnect(sesi);
        ssh_free(sesi);
        return 1;
    }

    printf("terhubung ke %s!\n\n", host);
    printf("peta tombol:\n");
    printf("  1 -> mode\n");
    printf("  2 -> start\n");
    printf("  3 -> user\n");
    printf("  4 -> mode_long\n");
    printf("  5 -> start_long\n");
    printf("  6 -> mode_long\n");
    printf("  q -> keluar\n\n");
    printf("mendengarkan keyboard...\n\n");
    fflush(stdout);

    /* ---- loop keyboard ------------------------------------------- */
    while (1) {
        char c = (char)_getch();  /* baca 1 karakter langsung tanpa enter */

        if (c == 'q' || c == 'Q') {
            printf("\nkeluar...\n");
            break;
        }

        if (c >= '1' && c <= '6') {
            int tombol = c - '0';
            publish_tombol(sesi, nama_tombol[tombol]);
        }
        /* tombol lain diabaikan */
    }

    /* ---- bersihkan ----------------------------------------------- */
    ssh_disconnect(sesi);
    ssh_free(sesi);
    printf("terputus. sampai jumpa!\n");
    return 0;
}
