/*
 * SonicArranger packed format converter
 *
 * This tool should restore a binary SonicArranger module to its original
 * format that can be loaded into SonicArranger again.
 *
 * by Thomas Meyer <mnemotron@gmail.com>
 * Created: 2022-06-02
 * Last change: 2022-06-03
 *
 * Format references:
 * http://old.exotica.org.uk/tunes/formats/sonic/SONIC_AR.TXT
 * https://github.com/Pyrdacor/SonicArranger/blob/main/Docs/PackedFileFormat.md
 *
 * This is not suppoed to be an example of good, structured programming.
 *
 * Written with SAS/C 6.5.
 * Compile with: sc sonicconv.c link
 */

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* struct for storing data per sample */
typedef struct {
    // length in bytes
    long length;
    // length in words
    long length_from_instr;
    // repeat in words
    long repeat_from_instr;
    // pointer to name entry in instrument table
    char* name_from_instr;
} sample_info;

char SOAR_ID [] = "SOARV1.0";
char STBL_ID [] = "STBL";
char OVTB_ID [] = "OVTB";
char NTBL_ID [] = "NTBL";
char INST_ID [] = "INST";
char SD8B_ID [] = "SD8B";
char SYWT_ID [] = "SYWT";
char SYAR_ID [] = "SYAR";
char SYAF_ID [] = "SYAF";
char EDAT_ID [] = "EDATV1.1";
char EDAT_DATA [] = { 0, 1, 0, 1, 0, 0, 0, 0x7b, 0, 0, 0, 0, 0, 1, 0, 3 };

/*
 * findsong - Find the offset off the song data.
 */
long findsong(char *data, long size) {
    long offset;
    long max_offset;
    long *p1, *p2;

    max_offset = size - 0x28;
    for(offset = 0; offset < max_offset; offset += 2) {
        p1 = (long *)(data + offset);
        p2 = p1 + 1;
        // We assume that offset 0x28 and a slightly larger value
        // after that is the beginning of the song data.
        if(*p1 == 0x28 && *p2 > *p1 && *p2 < 0x400)
            return offset;
    }

    return -1;
}

/*
 * convert - Do the actual conversion.
 */
int convert(char *in_name, char *out_name) {
    FILE *f_in, *f_out;
    long f_in_size;
    long read_len;
    char *dat;
    struct stat st;
    int rc;
    long i;
    long offset;
    long i_offset;
    long song_offset, over_offset, note_offset, instr_offset;
    long wave_offset, adsr_offset, amf_offset, sample_offset;
    long song_len, over_len, note_len, instr_len, wave_len;
    long adsr_len, amf_len, sample_len;
    long song_cnt, over_cnt, note_cnt, instr_cnt, wave_cnt;
    long adsr_cnt, amf_cnt, sample_cnt;
    short instr_sample_id;
    long s_len;
    long samples_info_size;
    sample_info (*samples_info)[];

    rc = stat(in_name, &st);
    if(rc == -1) {
        printf("stat error file: %s\n", in_name);
    }

    f_in_size = st.st_size;

    f_in = fopen(in_name, "rb");
    if(f_in == NULL) {
        printf("Cannot open input file: %s\n", in_name);
        return -1;
    }

    printf("Source size=0x%lx\n", f_in_size);
    dat = malloc(f_in_size);
    read_len = fread(dat, 1, f_in_size, f_in);
    fclose(f_in);

    if(f_in_size == read_len) {
        printf("Read 0x%lx bytes\n", read_len);
    }
    else {
        printf("Read failed. Expected 0x%08x, got 0x%08x bytes.\n", f_in_size, read_len);
    }

    offset = findsong(dat, f_in_size);
    if(offset >= 0 && offset < f_in_size - 28) {
        printf("Found module at 0x%lx\n", offset);

        song_offset = offset + *((long *)(dat + offset));
        over_offset = offset + *((long *)(dat + offset + 4));
        note_offset = offset + *((long *)(dat + offset + 8));
        instr_offset = offset + *((long *)(dat + offset + 12));
        wave_offset = offset + *((long *)(dat + offset + 16));
        adsr_offset = offset + *((long *)(dat + offset + 20));
        amf_offset = offset + *((long *)(dat + offset + 24));
        sample_offset = offset + *((long *)(dat + offset + 28));

        // TODO Check offsets for buffer overflow

        song_len = over_offset - song_offset;
        over_len = note_offset - over_offset;
        note_len = instr_offset - note_offset;
        instr_len = wave_offset - instr_offset;
        wave_len = adsr_offset - wave_offset;
        adsr_len = amf_offset - adsr_offset;
        amf_len = sample_offset - amf_offset;

        song_cnt = song_len / 12;
        over_cnt = over_len / 16;
        note_cnt = note_len / 4;
        instr_cnt = instr_len / 152;
        wave_cnt = wave_len / 128;
        adsr_cnt = adsr_len / 128;
        amf_cnt = amf_len / 128;
        sample_cnt = *((long *)(dat + sample_offset));

        samples_info_size = sample_cnt * sizeof(sample_info);
        samples_info = malloc(samples_info_size);

        if(samples_info != NULL) {
            memset(samples_info, 0, samples_info_size);

            sample_len = 0;
            for(i = 0; i < sample_cnt; i++) {
                s_len = *((long *)(dat + sample_offset + 4 + i * 4));
                sample_len += s_len;
                (*samples_info)[i].length = s_len;
            }

            // Gather info for samples from instrument entries
            i_offset = instr_offset;
            for(i = 0; i < instr_cnt; i++) {
                if(*((unsigned short*) (dat + i_offset)) == 0) {
                    // is sampled instrument
                    instr_sample_id = *((unsigned short*)(dat + i_offset + 2));

                    if(instr_sample_id >= 0 && instr_sample_id < sample_cnt) {
                        (*samples_info)[instr_sample_id].length_from_instr = (long) *((unsigned short*)(dat + i_offset + 4));
                        (*samples_info)[instr_sample_id].repeat_from_instr = (long) *((unsigned short*)(dat + i_offset + 6));
                        (*samples_info)[instr_sample_id].name_from_instr = dat + i_offset + 0x7a;
                    }
                    else {
                        printf("Inconsistent sample id 0x%04x in instrument %ld! Data ignored.\n", instr_sample_id, i);
                    }
                }
                i_offset += 0x98;
            }

            printf("song: 0x%08lx len=0x%08lx cnt=%ld\n", song_offset, song_len, song_cnt);
            printf("over: 0x%08lx len=0x%08lx cnt=%ld\n", over_offset, over_len, over_cnt);
            printf("note: 0x%08lx len=0x%08lx cnt=%ld\n", note_offset, note_len, note_cnt);
            printf("inst: 0x%08lx len=0x%08lx cnt=%ld\n", instr_offset, instr_len, instr_cnt);
            printf("wave: 0x%08lx len=0x%08lx cnt=%ld\n", wave_offset, wave_len, wave_cnt);
            printf("adsr: 0x%08lx len=0x%08lx cnt=%ld\n", adsr_offset, adsr_len, adsr_cnt);
            printf("amf : 0x%08lx len=0x%08lx cnt=%ld\n", amf_offset, amf_len, amf_cnt);
            printf("smpl: 0x%08lx len=0x%08lx cnt=%ld\n", sample_offset, sample_len, sample_cnt);
        
            f_out = fopen(out_name, "wb");
            if(f_out != 0) {
                fwrite(SOAR_ID, 1, 8, f_out);

                fwrite(STBL_ID, 1, 4, f_out);
                fwrite(&song_cnt, 1, 4, f_out);
                fwrite(dat + song_offset, 1, song_len, f_out);

                fwrite(OVTB_ID, 1, 4, f_out);
                fwrite(&over_cnt, 1, 4, f_out);
                fwrite(dat + over_offset, 1, over_len, f_out);

                fwrite(NTBL_ID, 1, 4, f_out);
                fwrite(&note_cnt, 1, 4, f_out);
                fwrite(dat + note_offset, 1, note_len, f_out);

                fwrite(INST_ID, 1, 4, f_out);
                fwrite(&instr_cnt, 1, 4, f_out);
                fwrite(dat + instr_offset, 1, instr_len, f_out);

                fwrite(SD8B_ID, 1, 4, f_out);
                fwrite(&sample_cnt, 1, 4, f_out);
                // write lengths, repeats, names, lengths
                for(i = 0; i < sample_cnt; i++) {
                    fwrite(&(*samples_info)[i].length_from_instr, 1, 4, f_out);
                }
                for(i = 0; i < sample_cnt; i++) {
                    fwrite(&(*samples_info)[i].repeat_from_instr, 1, 4, f_out);
                }
                for(i = 0; i < sample_cnt; i++) {
                    fwrite((*samples_info)[i].name_from_instr, 1, 30, f_out);
                }
                for(i = 0; i < sample_cnt; i++) {
                    fwrite(&(*samples_info)[i].length, 1, 4, f_out);
                }
                fwrite(dat + sample_offset + 4 + sample_cnt * 4, 1, sample_len, f_out);

                fwrite(SYWT_ID, 1, 4, f_out);
                fwrite(&wave_cnt, 1, 4, f_out);
                fwrite(dat + wave_offset, 1, wave_len, f_out);

                fwrite(SYAR_ID, 1, 4, f_out);
                fwrite(&adsr_cnt, 1, 4, f_out);
                fwrite(dat + adsr_offset, 1, adsr_len, f_out);

                fwrite(SYAF_ID, 1, 4, f_out);
                fwrite(&amf_cnt, 1, 4, f_out);
                fwrite(dat + amf_offset, 1, amf_len, f_out);

                fwrite(EDAT_ID, 1, 8, f_out);
                fwrite(EDAT_DATA, 1, 0x10, f_out);
                fclose(f_out);

                printf("Conversion written to: %s\n", out_name);
            }
            else
                printf("Cannot open file: %s\n", out_name);

            free(samples_info);
        }
        else {
            printf("Cannot allocate memory for samples info!\n");
        }
    }
    else {
        printf("Song not found in file: %s\n", in_name);
    }

    free(dat);

    return 0;
}

int main(int argc,char *argv[]) {
    printf("sonicconv -- SonicArranger packed format converter\n");
    printf("by Thomas Meyer <mnemotron@gmail.com>\n\n");

    if(argc < 3) {
        printf("Usage: sonicconv <inputfile> <outputfile>\n");
        exit(10);
    }

    convert(argv[1], argv[2]);
}
