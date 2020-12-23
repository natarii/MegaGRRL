#include "freertos/FreeRTOS.h"
#include "filebrowser.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../key.h"
#include "../player.h"
#include "../lcddma.h"
#include "../queue.h"
#include "../taskmgr.h"
#include "../ui.h"
#include "fwupdate.h"
#include <rom/crc.h>
#include <stdio.h>
#include <dirent.h>

#include "softbar.h"

static const char* TAG = "Ui_FileBrowser";



static IRAM_ATTR lv_obj_t *container;
lv_style_t filestyle;
lv_style_t filestyle_sel;
lv_style_t filelabelstyle_dir;
lv_style_t filelabelstyle_aud;
lv_style_t filelabelstyle_other;
lv_style_t filelabelstyle_fw;

IRAM_ATTR lv_obj_t *files[10];
IRAM_ATTR lv_obj_t *labels[10];
IRAM_ATTR lv_obj_t *icons[10];

IRAM_ATTR lv_obj_t *scrollbar;
lv_style_t scrollbarstyle;

uint16_t selectedfile = 0;
uint16_t selectedfile_last = 0xff;
uint16_t diroffset = 0;

char startpath[] = "/sd";
char path[264];
char temppath[264];
DIR *dir;
struct dirent *ent;

static char direntry_cache[FILEBROWSER_CACHE_SIZE];
static IRAM_ATTR uint32_t direntry_offset[FILEBROWSER_CACHE_MAXENTRIES];
static uint16_t direntry_count = 0;
static volatile bool direntry_invalidated = false;

void openselection();
void redrawselection();
void redrawlist();
void startdir(bool docache);
void backdir();

static const char thumbsdb[] = "Thumbs.db";
static const char sysvolinfo[] = "System Volume Information";
static const char recyclebin[] = "$Recycle.Bin";
#define BROWSER_IGNORE if (ent->d_name[0] == '.' || strcasecmp(ent->d_name, thumbsdb) == 0 || strcmp(ent->d_name, sysvolinfo) == 0 || strcmp(ent->d_name, recyclebin) == 0) continue;
#define BROWSER_LAST_VER 0x16

volatile SortDirection_t Ui_FileBrowser_SortDir = SORT_ASCENDING;
volatile bool Ui_FileBrowser_Sort = true;
volatile bool Ui_FileBrowser_DirsBeforeFiles = true;
volatile bool Ui_FileBrowser_Narrow = false;

void Ui_FileBrowser_InvalidateDirEntry() {
    direntry_invalidated = true;
}

static int comp(const void *offset1, const void *offset2) {
    const uint32_t *o1 = offset1, *o2 = offset2;
    char *s1 = direntry_cache + *o1;
    char *s2 = direntry_cache + *o2;
    unsigned char t1 = direntry_cache[*o1-1];
    unsigned char t2 = direntry_cache[*o2-1];
    if (Ui_FileBrowser_DirsBeforeFiles) if (t1 != t2) return t2-t1; //cheezy, but should be fine
    if (Ui_FileBrowser_SortDir == SORT_ASCENDING) {
        return strcasecmp(s1, s2);
    } else {
        return strcasecmp(s2, s1);
    }
}

void cachedir(char *dir_name) {
    ESP_LOGI(TAG, "generating dir cache for %s...", dir_name);
    uint32_t s = xthal_get_ccount();
    dir = opendir(dir_name);
    uint32_t off = 0;
    direntry_count = 0;
    while ((ent=readdir(dir))!=NULL) {
        BROWSER_IGNORE;
        direntry_cache[off] = ent->d_type;
        off += 1;
        direntry_offset[direntry_count] = off;
        strcpy(&direntry_cache[off], ent->d_name);
        off += strlen(ent->d_name) + 1;
        direntry_count++;
        if (off > FILEBROWSER_CACHE_SIZE) {
            ESP_LOGE(TAG, "filebrowser cache area over !!");
            closedir(dir);
            return;
        }
        if (direntry_count > FILEBROWSER_CACHE_MAXENTRIES) {
            ESP_LOGE(TAG, "filebrowser cache over !!");
            closedir(dir);
            return;
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "done after %d msec. %d entries, cache use %d", ((xthal_get_ccount()-s)/240000), direntry_count, off);
    if (Ui_FileBrowser_Sort) {
        qsort(direntry_offset, direntry_count, sizeof(uint32_t), comp);
        ESP_LOGI(TAG, "sorted");
    }
}

void savelast() {
    FILE *last;
    last = fopen("/sd/.mega/fbrowser.las", "w");
    uint8_t ver = BROWSER_LAST_VER;
    //ver no.
    fwrite(&ver, 1, 1, last);
    uint32_t crc = 0;
    fwrite(&crc, 4, 1, last); //just to alloc the space in the file, this isn't valid data yet
    char *name = direntry_cache + direntry_offset[diroffset + selectedfile];
    uint16_t s = strlen(path) + 1 + strlen(name);
    //full path including last played filename, and length thereof
    fwrite(&s, 2, 1, last);
    crc = crc32_le(crc, (uint8_t*)&s, 2);
    fwrite(path, strlen(path), 1, last);
    crc = crc32_le(crc, (uint8_t*)path, strlen(path));
    fwrite("/", 1, 1, last);
    s = '/'; //extremely cheezy
    crc = crc32_le(crc, (uint8_t*)&s, 1);
    fwrite(name, strlen(name), 1, last);
    crc = crc32_le(crc, (uint8_t*)name, strlen(name));
    //offset of just the last played filename
    s = strlen(path)+1;
    fwrite(&s, 2, 1, last);
    crc = crc32_le(crc, (uint8_t*)&s, 2);
    //write out the type, so we know what to check for when loading
    unsigned char type = direntry_cache[direntry_offset[diroffset + selectedfile]-1];
    fwrite(&type, 1, 1, last);
    crc = crc32_le(crc, &type, 1);
    fseek(last, 1, SEEK_SET);
    fwrite(&crc, 4, 1, last);
    fclose(last);
    ESP_LOGI(TAG, "dumped fbrowser.las");
}

bool loadhistory() {
    //this shit will ASPLODE if you're more than 8 dirs deep RIP
    //could rewrite it to iterate forward through the dirs starting at root
    FILE *last;
    last = fopen("/sd/.mega/fbrowser.las", "r");
    if (!last) {
        ESP_LOGW(TAG, "no filebrowser history exists");
        return false;
    }
    ESP_LOGI(TAG, "loading filebrowser history");
    uint8_t ver;
    fread(&ver,1,1,last);
    if (ver != BROWSER_LAST_VER) {
        ESP_LOGW(TAG, "version 0x%02x doesn't match 0x%02x", ver, BROWSER_LAST_VER);
        fclose(last);
        return false;
    }
    uint32_t filecrc;
    fread(&filecrc,4,1,last);
    uint32_t curpos = ftell(last);
    fseek(last, 0, SEEK_END);
    uint32_t remaining = ftell(last) - curpos;

    //load in the rest of the file to verify the crc
    fseek(last, curpos, SEEK_SET);
    fread(direntry_cache, 1, remaining, last); //don't bother bounds checking this, if the last file is bigger than the direntry cache array, something is suuuuuper wrong
    uint32_t crc = 0;
    crc = crc32_le(crc, (uint8_t*)direntry_cache, remaining);
    ESP_LOGD(TAG, "SavedCRC = %08x, ReadCRC = %08x", filecrc, crc);
    if (filecrc != crc) {
        ESP_LOGE(TAG, "History file is corrupt!!!");
        fclose(last);
        return false;
    }
    fseek(last, curpos, SEEK_SET); //back to where we were before. TODO just yank it all out of the direntry cache array since we have it there already anyway...

    uint16_t pathlen;
    fread(&pathlen,2,1,last); //length of last path + filename
    fread(&path,1,pathlen,last); //last path + filename
    uint16_t nameoff;
    fread(&nameoff,2,1,last); //offset of just the filename
    uint8_t type;
    fread(&type,1,1,last); //type of last selected/opened file
    fclose(last);
    path[pathlen] = 0; //terminate string
    ESP_LOGI(TAG, "loadhistory: checking if %s (type %d) still exists...", path, type);
    DIR *test;
    last = NULL;
    test = NULL;
    if (type == DT_DIR) {
        test = opendir(path);
    } else if (type == DT_REG) {
        //this includes a pretty nasty hack for handling vgzs that got extracted and overwritten
        //the last dir file is saved before the vgz is extracted, so it will be .vgz in the file but .vgm on disk
        //if someone has both .vgm and .vgz files with the same name it'll end up with the wrong one
        last = fopen(path, "r");
        if (!last && (path[pathlen-1] == 'z' || path[pathlen-1] == 'Z')) {
            ESP_LOGW(TAG, "vgz does not exist, checking for vgm");
            path[pathlen-1] -= 0x0d;
            last = fopen(path, "r");
        }
    } else {
        ESP_LOGE(TAG, "error: unknown type");
        return false;
    }
    if (last || test) {
        if (type == DT_DIR) {
            closedir(test);
        } else if (type == DT_REG) {
            fclose(last);
        }
        ESP_LOGI(TAG, "it does");
    } else {
        ESP_LOGW(TAG, "it does not");
        return false;
    }
    path[nameoff-1] = 0; //chop off the filename from the path, at the last /

    ESP_LOGI(TAG, "loadhistory: going back to %s, last selection was %s", path, &path[nameoff]);

    cachedir(path);
    for (uint16_t i=0;i<direntry_count;i++) {
        char *name = direntry_cache + direntry_offset[i];
        if (strcmp(name, &path[nameoff]) == 0) {
            diroffset = (i/10)*10;
            selectedfile = i%10;
            ESP_LOGI(TAG, "loadhistory done: found last selection at diroffset %d selfile %d", diroffset, selectedfile);
        }
    }

    ESP_LOGI(TAG, "all done");
    
    return true;
}

void Ui_FileBrowser_Setup() {
    if (loadhistory()) {
        selectedfile_last = selectedfile;
        //nothing more to do
    } else {
        strcpy(path, startpath);
        selectedfile_last = 0;
    }
}

static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updatescrollbar() {
    uint32_t toffset = diroffset + selectedfile;
    uint32_t df = direntry_count;
    if (df > 100) df = 100;
    if (df == 0) df = 1;
    uint32_t height = map(df, 1, 100, 250, 35);
    uint32_t heightleft = 250-height;
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    if (heightleft) {
        lv_obj_set_pos(scrollbar, 235, map(toffset, 0, direntry_count-1, 0, heightleft));
    } else { //heightleft = 0, 1 file in dir
        lv_obj_set_pos(scrollbar, 235, 0);
    }
    lv_obj_set_height(scrollbar, height);
    LcdDma_Mutex_Give();
}

bool Ui_FileBrowser_Activate(lv_obj_t *uiscreen) {

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    lv_style_copy(&filestyle, &lv_style_plain);
    filestyle.body.main_color = LV_COLOR_MAKE(0,0,0); //255,255,255);
    filestyle.body.grad_color = LV_COLOR_MAKE(0,0,0); //255,255,255);
    filestyle.text.font = Ui_FileBrowser_Narrow?&lv_font_dejavuC_18:&lv_font_dejavu_18;
    filestyle.text.color = LV_COLOR_MAKE(220,220,220);
    lv_style_copy(&filelabelstyle_dir, &filestyle);
    lv_style_copy(&filelabelstyle_aud, &filestyle);
    lv_style_copy(&filelabelstyle_other, &filestyle);
    lv_style_copy(&filelabelstyle_fw, &filestyle);
    filelabelstyle_dir.text.color = LV_COLOR_MAKE(0xff, 0xf7, 0x69);
    filelabelstyle_aud.text.color = LV_COLOR_MAKE(0x7f, 0xff, 0xff);
    filelabelstyle_other.text.color = LV_COLOR_MAKE(0x7f, 0x7f, 0x7f);
    filelabelstyle_fw.text.color = LV_COLOR_MAKE(0, 0xff, 0);
    lv_style_copy(&filestyle_sel, &filestyle);
    filestyle_sel.body.main_color = LV_COLOR_MAKE(0,0,100); //50,50,50);
    filestyle_sel.body.grad_color = LV_COLOR_MAKE(0,0,100); //50,50,50);
    filestyle_sel.text.color = LV_COLOR_MAKE(220,220,220); //255,255,255);
    filestyle_sel.body.radius = 8;
    lv_style_copy(&scrollbarstyle, &lv_style_plain);
    scrollbarstyle.body.main_color = LV_COLOR_MAKE(127,127,127);
    scrollbarstyle.body.grad_color = LV_COLOR_MAKE(127,127,127);
    filestyle.body.opa = 0; //must be after all the copies



    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 25*10);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);
    //lv_obj_set_hidden(container, true);

    for (uint8_t i=0;i<10;i++) {
        files[i] = lv_cont_create(container, NULL);
        lv_cont_set_style(files[i], LV_CONT_STYLE_MAIN, (i==selectedfile)?&filestyle_sel:&filestyle);
        lv_obj_set_height(files[i], 25);
        lv_obj_set_width(files[i],235);
        lv_obj_set_pos(files[i], 0, 25*i);
        labels[i] = lv_label_create(files[i], NULL);
        lv_obj_set_pos(labels[i], 26, 2);
        lv_label_set_text(labels[i], "");
        lv_label_set_long_mode(labels[i], (i==selectedfile)?Ui_GetScrollType():LV_LABEL_LONG_DOT);
        lv_label_set_anim_speed(labels[i], 50);
        lv_obj_set_width(labels[i], 205);
        icons[i] = lv_label_create(files[i], NULL);
        lv_obj_set_pos(icons[i], 4, 2);
        lv_label_set_text(icons[i], "");
        lv_obj_set_width(icons[i], 24);
    }

    scrollbar = lv_obj_create(container, NULL);
    lv_obj_set_style(scrollbar, &scrollbarstyle);
    lv_obj_set_height(scrollbar, 250);
    lv_obj_set_width(scrollbar, 5);
    lv_obj_set_pos(scrollbar, 235, 0);


    Ui_SoftBar_Update(0, true, LV_SYMBOL_HOME "Home", false);
    //Ui_SoftBar_Update(1, false, LV_SYMBOL_DIRECTORY" Up "LV_SYMBOL_UP);
    //Ui_SoftBar_Update(2, true, LV_SYMBOL_"Open");
    LcdDma_Mutex_Give();

    if (direntry_invalidated) {
        direntry_invalidated = false;
        //if model sort option changes are ever allowed, just move the cache+loop to its own function and reuse it there
        ESP_LOGI(TAG, "direntry was invalidated, re-caching. checking if last file still exists...");
        //another janky hack for vgz->vgm, like when loading history
        strcpy(temppath, path);
        strcat(temppath, "/");
        strcat(temppath, direntry_cache + direntry_offset[diroffset+selectedfile]);
        FILE *test = fopen(temppath, "r");
        bool vgztovgm = false;
        if (!test && (temppath[strlen(temppath)-1] == 'z' || temppath[strlen(temppath)-1] == 'Z')) {
            ESP_LOGW(TAG, "vgz does not exist, checking for vgm");
            vgztovgm = true;
        }
        if (test) fclose(test);
        ESP_LOGI(TAG, "current path: %s", path);
        strcpy(temppath, direntry_cache + direntry_offset[diroffset+selectedfile]);
        if (vgztovgm) temppath[strlen(temppath)-1] -= 0x0d;
        ESP_LOGI(TAG, "currently selected: %s", temppath);
        cachedir(path);
        for (uint16_t i=0;i<direntry_count;i++) {
            char *name = direntry_cache + direntry_offset[i];
            if (strcmp(name, temppath) == 0) {
                diroffset = (i/10)*10;
                selectedfile = i%10;
                ESP_LOGI(TAG, "found last selection at diroffset %d selfile %d", diroffset, selectedfile);
            }
        }
        startdir(false);
    } else {
        startdir(true);
    }

    return true;
}

void Ui_FileBrowser_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void redrawlistsel(bool list, bool sel) {
    uint8_t u=0;
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    for (uint16_t i=diroffset;i<diroffset+10;i++) {
        if (i == direntry_count) break; //prevent overflow if less than 10 files left in dir
        char *name = direntry_cache + direntry_offset[i];
        unsigned char type = direntry_cache[direntry_offset[i]-1];
        if (list) {
            if (type == DT_DIR) {
                lv_label_set_static_text(icons[u], LV_SYMBOL_DIRECTORY);
                //lv_label_set_style(labels[u], &filelabelstyle_dir);
                lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_dir);
            } else if (type == DT_REG) {
                uint8_t namelen = strlen(name);
                if (namelen > 4) {
                    if ((strcasecmp(&name[namelen-4], ".vgm") == 0) || (strcasecmp(&name[namelen-4], ".vgz") == 0)) {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_AUDIO);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_aud);
                    } else if (strcasecmp(&name[namelen-4], ".jpg") == 0 || strcasecmp(&name[namelen-4], ".png") == 0) {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_IMAGE);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_other);
                    } else if (strcasecmp(&name[namelen-4], ".txt") == 0) {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_EDIT);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_other);
                    } else if (strcasecmp(&name[namelen-4], ".m3u") == 0) {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_LIST);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_aud);
                    } else if (strcasecmp(&name[namelen-4], ".mgu") == 0) {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_DOWNLOAD);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_fw);
                    } else {
                        lv_label_set_static_text(icons[u], LV_SYMBOL_FILE);
                        lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_other);
                    }
                } else {
                    lv_label_set_static_text(icons[u], LV_SYMBOL_FILE);
                    lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_other);
                }
            } else {
                lv_label_set_static_text(icons[u], LV_SYMBOL_FILE);
                lv_label_set_style(icons[u], LV_LABEL_STYLE_MAIN, &filelabelstyle_other);
            }
            lv_label_set_text(labels[u], name); //can't use lv_label_set_static_text here - it writes to the passed string to truncate it
        }
        if (sel) {
            if (u == selectedfile) {
                if (type == DT_DIR) {
                    Ui_SoftBar_Update(2, true, LV_SYMBOL_RIGHT" Open", false);
                } else if (type == DT_REG) {
                    uint8_t namelen = strlen(name);
                    if (namelen > 4) {
                        char exten[5];
                        strcpy(exten, name + namelen - 4);
                        if ((strcasecmp(exten, ".vgm") == 0) || (strcasecmp(exten, ".vgz") == 0)) {
                            Ui_SoftBar_Update(2, true, LV_SYMBOL_PLAY" Play", false);
                        } else if (strcasecmp(&name[namelen-4], ".m3u") == 0) {
                            Ui_SoftBar_Update(2, true, LV_SYMBOL_PLAY" Play", false);
                        } else if (strcasecmp(&name[namelen-4], ".mgu") == 0) {
                            Ui_SoftBar_Update(2, true, LV_SYMBOL_CHARGE" Flash", false);
                        } else {
                            Ui_SoftBar_Update(2, false, LV_SYMBOL_CLOSE" N/A", false);
                        }
                    } else {
                        Ui_SoftBar_Update(2, false, LV_SYMBOL_CLOSE" N/A", false);
                    }
                }
            }
        }
        u++;
    }
    if (sel && !direntry_count) { //nonexistent file selected
        Ui_SoftBar_Update(2, false, LV_SYMBOL_CLOSE" N/A", false);
    }
    if (list && u < 10) {
        for (uint8_t i=u;i<10;i++) {
            lv_label_set_static_text(labels[i], "");
            lv_label_set_static_text(icons[i], "");
        }
    }
    if (sel) {
        lv_cont_set_style(files[selectedfile_last], LV_CONT_STYLE_MAIN, &filestyle);
        lv_label_set_long_mode(labels[selectedfile_last], LV_LABEL_LONG_DOT);
        lv_cont_set_style(files[selectedfile], LV_CONT_STYLE_MAIN, &filestyle_sel);
        lv_label_set_long_mode(labels[selectedfile], Ui_GetScrollType());
        selectedfile_last = selectedfile;
    }
    LcdDma_Mutex_Give();
}

void opendirectory() {
    ESP_LOGI(TAG, "browser enter %s", path);

    diroffset = 0;
    selectedfile = 0;

    startdir(true);
}

uint8_t dumpm3u() {
    uint16_t m = 0;
    uint16_t r = 0;
    FILE *p;
    p = fopen("/sd/.mega/temp.m3u", "w");
    for (uint16_t i=0;i<direntry_count;i++) {
        char *name = direntry_cache + direntry_offset[i];
        unsigned char type = direntry_cache[direntry_offset[i]-1];
        if (type == DT_REG && ((strcasecmp(&name[strlen(name)-4], ".vgm") == 0) || (strcasecmp(&name[strlen(name)-4], ".vgz") == 0))) {
            strcpy(temppath, path);
            strcat(temppath, "/");
            strcat(temppath, name);
            strcat(temppath, "\n");
            fwrite(temppath, strlen(temppath), 1, p);
            if (i == diroffset + selectedfile) r = m;
            m++;
        }
    }
    fclose(p);
    return r;
}

void m3u2m3u() {
    FILE *p;
    p = fopen("/sd/.mega/temp.m3u", "w");

    QueueLoadM3u(path, temppath, 0, true, false); //don't care about position after shuffle, we don't shuffle here ever
    for (uint32_t i=0;i<QueueLength;i++) {
        QueueSetupEntry(true, false);
        strcat(QueuePlayingFilename, "\n"); //VILE
        fwrite(QueuePlayingFilename, strlen(QueuePlayingFilename), 1, p);
        QueueNext();
    }

    fclose(p);
}

void openselection() {
    char *name = direntry_cache + direntry_offset[diroffset + selectedfile];
    unsigned char type = direntry_cache[direntry_offset[diroffset + selectedfile]-1];
    if (type == DT_DIR) {
        strcat(path, "/");
        strcat(path, name);
        opendirectory();
        return;
    } else if (type == DT_REG) {
        if ((strcasecmp(&name[strlen(name)-4], ".vgm") == 0) || (strcasecmp(&name[strlen(name)-4], ".vgz") == 0)) {
            ESP_LOGI(TAG, "playing vgm");
            ESP_LOGI(TAG, "request stop");
            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
            ESP_LOGI(TAG, "wait stop");
            xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
            ESP_LOGI(TAG, "load m3u");
            QueueLoadM3u("/sd/.mega", "/sd/.mega/temp.m3u", dumpm3u(), false, true);
            ESP_LOGI(TAG, "request start");
            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_START_RUNNING, eSetValueWithoutOverwrite);
            Ui_Screen = UISCREEN_NOWPLAYING;
            /*ESP_LOGI(TAG, "wait start");
            xEventGroupWaitBits(Player_Status, PLAYER_STATUS_RUNNING, false, true, pdMS_TO_TICKS(3000));*/
            ESP_LOGI(TAG, "ok");
            savelast();
        } else if (strcasecmp(&name[strlen(name)-4], ".m3u") == 0) {
            ESP_LOGI(TAG, "playing m3u");
            ESP_LOGI(TAG, "request stop");
            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
            ESP_LOGI(TAG, "wait stop");
            xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
            ESP_LOGI(TAG, "updating m3u");
            strcpy(temppath, path);
            strcat(temppath, "/");
            strcat(temppath, name);
            m3u2m3u();
            ESP_LOGI(TAG, "load m3u");
            QueueLoadM3u("/sd/.mega", "/sd/.mega/temp.m3u", 0, false, true);
            ESP_LOGI(TAG, "request start");
            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_START_RUNNING, eSetValueWithoutOverwrite);
            Ui_Screen = UISCREEN_NOWPLAYING;
            /*ESP_LOGI(TAG, "wait start");
            xEventGroupWaitBits(Player_Status, PLAYER_STATUS_RUNNING, false, true, pdMS_TO_TICKS(3000));*/
            ESP_LOGI(TAG, "ok");
            savelast();
        } else if (strcasecmp(&name[strlen(name)-4], ".mgu") == 0) {
            strcpy(temppath, path);
            strcat(temppath, "/");
            strcat(temppath, name);
            fwupdate_file = temppath;
            Ui_Screen = UISCREEN_FWUPDATE;
        } else {
            ESP_LOGE(TAG, "invalid file type not played");
        }
        return;
    }
}

void backdir() {
    uint16_t l = 0xffff;
    for (uint16_t i=0;i<strlen(path);i++) {
        if (path[i] == '/') l = i;
    }
    path[l] = 0;

    ESP_LOGI(TAG, "browser going back to %s, last selection was %s", path, &path[l+1]);

    cachedir(path);
    for (uint16_t i=0;i<direntry_count;i++) {
        char *name = direntry_cache + direntry_offset[i];
        if (strcmp(name, &path[l+1]) == 0) {
            diroffset = (i/10)*10;
            selectedfile = i%10;
            ESP_LOGI(TAG, "found last selection at diroffset %d selfile %d", diroffset, selectedfile);
        }
    }

    startdir(false);
}

void startdir(bool docache) {
    if (docache) cachedir(path);
    redrawlistsel(true, true);
    updatescrollbar();
    Ui_SoftBar_Update(1, strcmp(path, startpath) != 0, LV_SYMBOL_UP" "LV_SYMBOL_DIRECTORY"Up", true);
}

void Ui_FileBrowser_Key(KeyEvent_t event) {
    bool sel = false;
    bool list = false;
    if (event.State & KEY_EVENT_PRESS) {
        if (event.Key == KEY_UP) {
            if (selectedfile == 0 && diroffset == 0 && direntry_count) {
                diroffset = (direntry_count - 1) / 10 * 10;
                if (direntry_count - diroffset <= 9) {
                    selectedfile = direntry_count - diroffset - 1;
                } else {
                    selectedfile = 9;
                }
                ESP_LOGI(TAG, "last pg");
                sel = list = true;
            } else if (selectedfile > 0) {
                selectedfile--;
                sel = true;
            } else if (diroffset > 0) {
                diroffset -= 10;
                selectedfile = 9;
                ESP_LOGI(TAG, "prev pg");
                sel = list = true;
            }
        } else if (event.Key == KEY_DOWN) {
            if (diroffset + selectedfile + 1 >= direntry_count) {
                diroffset = selectedfile = 0;
                ESP_LOGI(TAG, "first pg");
                sel = list = true;
            } else if (selectedfile < 9) {
                if (diroffset + selectedfile + 1 /*+1, would normally do direntry_count-1, but avoids potentially underflowing direntry_count*/ < direntry_count) {
                    selectedfile++;
                    sel = true;
                }
            } else if (diroffset + selectedfile + 1 < direntry_count) {
                diroffset += 10;
                selectedfile = 0;
                ESP_LOGI(TAG, "next pg");
                sel = list = true;
            }
        } else if (event.Key == KEY_RIGHT) { //todo: broken
            if (selectedfile < 9 && diroffset + selectedfile + 1 /*+1, would normally do direntry_count-1, but avoids potentially underflowing direntry_count*/ < direntry_count) {
                if (direntry_count - diroffset <= 9) {
                    selectedfile = direntry_count - diroffset - 1;
                } else {
                    selectedfile = 9;
                }
                sel = true;
            } else if (diroffset + selectedfile + 1 < direntry_count) {
                diroffset += 10;
                if (direntry_count - diroffset <= 9) {
                    selectedfile = direntry_count - diroffset - 1;
                } else {
                    selectedfile = 9;
                }
                sel = list = true;
            }
        } else if (event.Key == KEY_LEFT) {
            if (selectedfile > 0) {
                selectedfile = 0;
                sel = true;
            } else if (diroffset > 0) {
                selectedfile = 0;
                diroffset -= 10;
                sel = list = true;
            }
        } else if (event.Key == KEY_C) {
            KeyMgr_Consume(KEY_C);
            if (direntry_count) {
                openselection();
            }
        } else if (event.Key == KEY_B) {
            KeyMgr_Consume(KEY_B);
            if (strcmp(path, startpath) != 0) backdir();
        } else if (event.Key == KEY_A) {
            KeyMgr_Consume(KEY_A);
            Ui_Screen = UISCREEN_MAINMENU;
        }

        if (list || sel) { //if page or selection was changed
            redrawlistsel(list, sel);
            updatescrollbar();
        }
    }
}

void Ui_FileBrowser_Tick() {
    //unused atm
}
