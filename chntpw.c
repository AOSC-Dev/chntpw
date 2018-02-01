/*
 * chntpw.c - Offline Password Edit Utility for Windows SAM database
 *
 * This program uses the "ntreg" library to load and access the registry,
 * the "libsam" library for user / group handling
 * it's main purpose is to reset password based information.
 * It can also call the registry editor etc
 
 * 2013-may: Added group add/remove in user edit (using new functions
 *           in sam library)
 * 2013-apr: Changed around a bit on some features, chntpw is now
 *           mainly used for interactive edits.
 *           For automatic/scripted functions, use new programs:
 *           sampasswd and samusrgrp !
 * 2011-apr: Command line options added for hive expansion safe mode
 * 2010-jun: Syskey not visible in menu, but is selectable (2)
 * 2010-apr: Interactive menu adapts to show most relevant
 *           selections based on what is loaded
 * 2008-mar: Minor other tweaks
 * 2008-mar: Interactive reg ed moved out of this file, into edlib.c
 * 2008-mar: 64 bit compatible patch by Mike Doty, via Alon Bar-Lev
 *           http://bugs.gentoo.org/show_bug.cgi?id=185411
 * 2007-sep: Group handling extended, promotion now public
 * 2007-sep: User edit menu, some changes to user info edit
 * 2007-apr-may: Get and display users group memberships
 * 2007-apr: GNU license. Some bugfixes. Cleaned up some output.
 * 2004-aug: More stuff in regedit. Stringinput bugfixes.
 * 2004-jan: Changed some of the verbose/debug stuff
 * 2003-jan: Changed to use more of struct based V + some small stuff
 * 2003-jan: Support in ntreg for adding keys etc. Editor updated.
 * 2002-dec: New option: Specify user using RID
 * 2002-dec: New option: blank the pass (zero hash lengths).
 * 2001-jul: extra blank password logic (when NT or LANMAN hash missing)
 * 2001-jan: patched & changed to use OpenSSL. Thanks to Denis Ducamp
 * 2000-jun: changing passwords regardless of syskey.
 * 2000-jun: syskey disable works on NT4. Not properly on NT5.
 * 2000-jan: Attempt to detect and disable syskey
 * 1999-feb: Now able to browse registry hives. (write support to come)
 * See HISTORY.txt for more detailed info on history.
 *
 *****
 *
 * Copyright (c) 1997-2014 Petter Nordahl-Hagen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See file GPL.txt for the full license.
 * 
 *****
 *
 * Information and ideas taken from pwdump by Jeremy Allison.
 *
 * More info from NTCrack by Jonathan Wilkins.
 * 
 */ 

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>

/* Define DOCRYPTO in makefile to include cryptostuff to be able to change passwords to
 * a new one.
 * Changing passwords is seems not to be working reliably on XP and newer anyway.
 * When not defined, only reset (nulling) of passwords available.
 */

#ifdef DOCRYPTO
#include <openssl/des.h>
#include <openssl/md4.h>
#endif

#define uchar u_char
#define MD4Init MD4_Init
#define MD4Update MD4_Update
#define MD4Final MD4_Final

#include "ntreg.h"
#include "sam.h"

const char chntpw_version[] = "chntpw version 1.00 140201, (c) Petter N Hagen";

extern char *val_types[REG_MAX+1];

/* Global verbosity */
int gverbose = 0;


#define MAX_HIVES 10

/* Array of loaded hives */
struct hive *hive[MAX_HIVES+1];
int no_hives = 0;

/* Icky icky... globals used to refer to hives, will be
 * set when loading, so that hives can be loaded in any order
 */

int H_SAM = -1;
int H_SYS = -1;
int H_SEC = -1;
int H_SOF = -1;

int syskeyreset = 0;
int dirty = 0;
int max_sam_lock = 0;

/* ============================================================== */


#ifdef DOCRYPTO

/* Crypto-stuff & support for what we'll do in the V-value */

/* Zero out string for lanman passwd, then uppercase
 * the supplied password and put it in here */

void make_lanmpw(char *p, char *lm, int len)
{
   int i;
   
   for (i=0; i < 15; i++) lm[i] = 0;
   for (i=0; i < len; i++) lm[i] = toupper(p[i]);
}

/*
 * Convert a 7 byte array into an 8 byte des key with odd parity.
 */

void str_to_key(unsigned char *str,unsigned char *key)
{
	int i;

	key[0] = str[0]>>1;
	key[1] = ((str[0]&0x01)<<6) | (str[1]>>2);
	key[2] = ((str[1]&0x03)<<5) | (str[2]>>3);
	key[3] = ((str[2]&0x07)<<4) | (str[3]>>4);
	key[4] = ((str[3]&0x0F)<<3) | (str[4]>>5);
	key[5] = ((str[4]&0x1F)<<2) | (str[5]>>6);
	key[6] = ((str[5]&0x3F)<<1) | (str[6]>>7);
	key[7] = str[6]&0x7F;
	for (i=0;i<8;i++) {
		key[i] = (key[i]<<1);
	}
	DES_set_odd_parity((des_cblock *)key);
}

/*
 * Function to convert the RID to the first decrypt key.
 */

void sid_to_key1(uint32_t sid,unsigned char deskey[8])
{
	unsigned char s[7];

	s[0] = (unsigned char)(sid & 0xFF);
	s[1] = (unsigned char)((sid>>8) & 0xFF);
	s[2] = (unsigned char)((sid>>16) & 0xFF);
	s[3] = (unsigned char)((sid>>24) & 0xFF);
	s[4] = s[0];
	s[5] = s[1];
	s[6] = s[2];

	str_to_key(s,deskey);
}

/*
 * Function to convert the RID to the second decrypt key.
 */

void sid_to_key2(uint32_t sid,unsigned char deskey[8])
{
	unsigned char s[7];
	
	s[0] = (unsigned char)((sid>>24) & 0xFF);
	s[1] = (unsigned char)(sid & 0xFF);
	s[2] = (unsigned char)((sid>>8) & 0xFF);
	s[3] = (unsigned char)((sid>>16) & 0xFF);
	s[4] = s[0];
	s[5] = s[1];
	s[6] = s[2];

	str_to_key(s,deskey);
}

/* DES encrypt, for LANMAN */

void E1(uchar *k, uchar *d, uchar *out)
{
  des_key_schedule ks;
  des_cblock deskey;

  str_to_key(k,(uchar *)deskey);
#ifdef __FreeBSD__
  des_set_key(&deskey,ks);
#else /* __FreeBsd__ */
  des_set_key((des_cblock *)deskey,ks);
#endif /* __FreeBsd__ */
  des_ecb_encrypt((des_cblock *)d,(des_cblock *)out, ks, DES_ENCRYPT);
}

#endif   /* DOCRYPTO */



/* Promote user into administrators group (group ID 0x220)
 * rid   - users rid
 * no returns yet
 */

void promote_user(int rid)
{

  char yn[5];

  if (!rid || (H_SAM < 0)) return;
  
  printf("\n=== PROMOTE USER\n\n");
  printf("Will add the user to the administrator group (0x220)\n"
	 "and to the users group (0x221). That should usually be\n"
	 "what is needed to log in and get administrator rights.\n"
	 "Also, remove the user from the guest group (0x222), since\n"
	 "it may forbid logins.\n\n");
  printf("(To add or remove user from other groups, please other menu selections)\n\n");
  printf("Note: You may get some errors if the user is already member of some\n"
	 "of these groups, but that is no problem.\n\n");

  fmyinput("Do it? (y/n) [n] : ", yn, 3);

  if (*yn == 'y') {

    printf("* Adding to 0x220 (Administrators) ...\n");
    sam_add_user_to_grp(hive[H_SAM], rid, 0x220);
    printf("* Adding to 0x221 (Users) ...\n");
    sam_add_user_to_grp(hive[H_SAM], rid, 0x221);

    printf("* Removing from 0x222 (Guests) ...\n");
    sam_remove_user_from_grp(hive[H_SAM], rid, 0x222);

    printf("\nPromotion DONE!\n");

  } else {
    printf("Nothing done, going back..\n");
  }

}


void interactive_remusrgrp(int rid)
{
  char inp[20];
  int grp;

  printf("\n=== REMOVE USER FROM A GROUP\n");

  sam_list_user_groups(hive[H_SAM], rid,0);

  printf("\nPlease enter group number (for example 220), or 0 to go back\n");
  fmyinput("Group number? : ",inp,16);
  sscanf(inp, "%x", &grp);

  if (!grp) {
    printf("Going back..\n");
    return;
  }

  printf("Removing user from group 0x%x (%d)\n",grp,grp);
  printf("Error messages if the user was not member of the group are harmless\n\n");

  sam_remove_user_from_grp(hive[H_SAM], rid, grp);

  printf("\nFinished removing user from group\n\n");

}


void interactive_addusrgrp(int rid)
{
  char inp[20];
  int grp;

  printf("\n == ADD USER TO A GROUP\n");

  sam_list_groups(hive[H_SAM], 0, 1);

  printf("\nPlease enter group number (for example 220), or 0 to go back\n");
  fmyinput("Group number? : ",inp,16);
  sscanf(inp, "%x", &grp);

  if (!grp) {
    printf("Going back..\n");
    return;
  }

  printf("Adding user to group 0x%x (%d)\n",grp,grp);
  printf("Error messages if the user was already member of the group are harmless\n\n");

  sam_add_user_to_grp(hive[H_SAM], rid, grp);

  printf("\nFinished adding user to group\n\n");


}


/* Unbind user from Internet credential provider (e.g. Microsoft Accounts)
 * rid   - users rid
 * no returns yet
 */

void unbind_internetprovider(int rid)
{

  char yn[5];

  if (!rid || (H_SAM < 0)) return;
  
  printf("\n=== UNBIND USER FROM INTERNET CREDENTIAL PROVIDER\n\n");
  printf("Will unbind the user from any Internet credential providers\n"
	 "it may be bound to, such as Microsoft Accounts.\n"
	 "The user's login credentials will then be stored locally in the SAM.\n\n"
	 "It's recommended to clear the password after doing this.\n\n");

  fmyinput("Do it? (y/n) [n] : ", yn, 3);

  if (*yn == 'y') {

    printf("* Unbinding from Internet credential provider ...\n");
    sam_unbind_user_from_provider(hive[H_SAM], rid);

    printf("\nUnbind DONE!\n");

  } else {
    printf("Nothing done, going back..\n");
  }

}

unsigned short change_accountbits(unsigned short acb) {
  char input[20];
  int bit = 15, pl = 0;

change_acb_init:
  sam_display_accountbits(acb);
  puts("First line shows bits 0-2, followed by bits 3-5.");

  while (1) {
    printf("- - - - Toggling account bits:\n");
    printf("Hex from 0 to e - toggle this bit\n");
    printf("d - Re-explain account bits\n");
    printf("q - Quit editing account bits\n");

    pl = fmyinput("Select: [q] > ", input, 16);

    if ('A' <= *input && *input <= 'Z') {
      *input -= ('A' - 'a');
    }

    if ((pl < 1) || (*input == 'q')) return acb;

    if (*input == 'd') goto change_acb_init;

    if (*input <= '9' && *input >= '0') {
      bit = *input - '0';
    } else if ('a' <= *input && *input <= 'e') {
      bit = 0xa + *input - 'a';
    } else {
      puts ("Pardon me?");
      continue;
    }

    acb ^= 1u << bit;
  }
  return acb;
}

/* Decode the V-struct, and change the password
 * vofs - offset into SAM buffer, start of V struct
 * rid - the users RID, required for the DES decrypt stage
 *
 * Some of this is ripped & modified from pwdump by Jeremy Allison
 * 
 */
char *change_pw(char *buf, int rid, int vlen, int stat)
{
   
   int pl;
   char *vp;
   static char username[128],fullname[128];
   char comment[128], homedir[128], newp[20];
   int username_offset,username_len;
   int fullname_offset,fullname_len;
   int comment_offset,comment_len;
   int homedir_offset,homedir_len;
   int ntpw_len,lmpw_len,ntpw_offs,lmpw_offs;
   unsigned short acb;
   struct user_V *v;

#ifdef DOCRYPTO
   int dontchange = 0;
   int i;
   char md4[32],lanman[32];
   char newunipw[34], despw[20], newlanpw[16], newlandes[20];
   des_key_schedule ks1, ks2;
   des_cblock deskey1, deskey2;
   MD4_CTX context;
   unsigned char digest[16];
   uchar x1[] = {0x4B,0x47,0x53,0x21,0x40,0x23,0x24,0x25};
#endif


   while (1) {  /* Loop until quit input */

   v = (struct user_V *)buf;
   vp = buf;
 
   username_offset = v->username_ofs;
   username_len    = v->username_len; 
   fullname_offset = v->fullname_ofs;
   fullname_len    = v->fullname_len;
   comment_offset  = v->comment_ofs;
   comment_len     = v->comment_len;
   homedir_offset  = v->homedir_ofs;
   homedir_len     = v->homedir_len;
   lmpw_offs       = v->lmpw_ofs;
   lmpw_len        = v->lmpw_len;
   ntpw_offs       = v->ntpw_ofs;
   ntpw_len        = v->ntpw_len;

   if (!rid) {
     printf("No RID given. Unable to change passwords..\n");
     return(0);
   }

   if (gverbose) {
     printf("lmpw_offs: 0x%x, lmpw_len: %d (0x%x)\n",lmpw_offs,lmpw_len,lmpw_len);
     printf("ntpw_offs: 0x%x, ntpw_len: %d (0x%x)\n",ntpw_offs,ntpw_len,ntpw_len);
   }

   *username = 0;
   *fullname = 0;
   *comment = 0;
   *homedir = 0;
   
   if(username_len <= 0 || username_len > vlen ||
      username_offset <= 0 || username_offset >= vlen ||
      comment_len < 0 || comment_len > vlen   ||
      fullname_len < 0 || fullname_len > vlen ||
      homedir_offset < 0 || homedir_offset >= vlen ||
      comment_offset < 0 || comment_offset >= vlen ||
      lmpw_offs < 0 || lmpw_offs >= vlen)
     {
	if (stat != 1) printf("change_pw: Not a legal V struct? (negative struct lengths)\n");
	return(NULL);
     }

   /* Offsets in top of struct is relative to end of pointers, adjust */
   username_offset += 0xCC;
   fullname_offset += 0xCC;
   comment_offset += 0xCC;
   homedir_offset += 0xCC;
   ntpw_offs += 0xCC;
   lmpw_offs += 0xCC;
   
   cheap_uni2ascii(vp + username_offset,username,username_len);
   cheap_uni2ascii(vp + fullname_offset,fullname,fullname_len);
   cheap_uni2ascii(vp + comment_offset,comment,comment_len);
   cheap_uni2ascii(vp + homedir_offset,homedir,homedir_len);
   
#ifdef SYSKEY
   /* Reset hash-lengths to 16 if syskey has been reset */
   if (syskeyreset && ntpw_len > 16 && !stat) {
     ntpw_len = 16;
     lmpw_len = 16;
     ntpw_offs -= 4;
     (unsigned int)*(vp+0xa8) = ntpw_offs - 0xcc;
     *(vp + 0xa0) = 16;
     *(vp + 0xac) = 16;
   }
#endif

   printf("================= USER EDIT ====================\n");
   printf("\nRID     : %04d [%04x]\n",rid,rid);
   printf("Username: %s\n",username);
   printf("fullname: %s\n",fullname);
   printf("comment : %s\n",comment);
   printf("homedir : %s\n\n",homedir);
   
   sam_list_user_groups(hive[H_SAM], rid,0);
   printf("\n");

   acb = sam_handle_accountbits(hive[H_SAM], rid,1);

   if (lmpw_len < 16 && gverbose) {
      printf("** LANMAN password not set. User MAY have a blank password.\n** Usually safe to continue. Normal in Vista\n");
   }

   if (ntpw_len < 16) {
      printf("** No NT MD4 hash found. This user probably has a BLANK password!\n");
      if (lmpw_len < 16) {
	printf("** No LANMAN hash found either. Try login with no password!\n");
#ifdef DOCRYPTO
	dontchange = 1;
#endif
      } else {
	printf("** LANMAN password IS however set. Will now install new password as NT pass instead.\n");
	printf("** NOTE: Continue at own risk!\n");
	ntpw_offs = lmpw_offs;
	*(vp+0xa8) = ntpw_offs - 0xcc;
	ntpw_len = 16;
	lmpw_len = 0;
      }
   }
   
   if (gverbose) {
     hexprnt("Crypted NT pw: ",(unsigned char *)(vp+ntpw_offs),16);
     hexprnt("Crypted LM pw: ",(unsigned char *)(vp+lmpw_offs),16);
   }

#ifdef DOCRYPTO
   /* Get the two decrpt keys. */
   sid_to_key1(rid,(unsigned char *)deskey1);
   des_set_key((des_cblock *)deskey1,ks1);
   sid_to_key2(rid,(unsigned char *)deskey2);
   des_set_key((des_cblock *)deskey2,ks2);
   
   /* Decrypt the NT md4 password hash as two 8 byte blocks. */
   des_ecb_encrypt((des_cblock *)(vp+ntpw_offs ),
		   (des_cblock *)md4, ks1, DES_DECRYPT);
   des_ecb_encrypt((des_cblock *)(vp+ntpw_offs + 8),
		   (des_cblock *)&md4[8], ks2, DES_DECRYPT);

   /* Decrypt the lanman password hash as two 8 byte blocks. */
   des_ecb_encrypt((des_cblock *)(vp+lmpw_offs),
		   (des_cblock *)lanman, ks1, DES_DECRYPT);
   des_ecb_encrypt((des_cblock *)(vp+lmpw_offs + 8),
		   (des_cblock *)&lanman[8], ks2, DES_DECRYPT);
      
   if (gverbose) {
     hexprnt("MD4 hash     : ",(unsigned char *)md4,16);
     hexprnt("LANMAN hash  : ",(unsigned char *)lanman,16);
   }
#endif  /* DOCRYPTO */


   printf("\n- - - - User Edit Menu:\n");
   printf(" 1 - Clear (blank) user password\n");
   printf("%s2 - Unlock and enable user account%s\n", (acb & 0x8000) ? " " : "(", 
	  (acb & 0x8000) ? " [probably locked now]" : ") [seems unlocked already]");
   printf(" 3 - Promote user (make user an administrator)\n");
   printf(" 4 - Add user to a group\n");
   printf(" 5 - Remove user from a group\n");
   printf(" 6 - Unbind user from Internet credential provider (e.g. Microsoft Accounts)\n");
#ifdef DOCRYPTO
   printf(" 9 - Edit (set new) user password (careful with this on XP or Vista)\n");
#endif
   printf(" s - Manually set individual account bits (e.g. re-lock)\n");
   printf(" e - Manually set all account bits (advanced)\n");
   printf(" q - Quit editing user, back to user select\n");

   pl = fmyinput("Select: [q] > ",newp,16);

   if ( (pl < 1) || (*newp == 'q') || (*newp == 'Q')) return(0);

   // One day this will become switch-case too. Uhhh.
   if (*newp == '2') {
     acb = sam_handle_accountbits(hive[H_SAM], rid,2);
     // return(username);
   }

   if (*newp == '3') {
     promote_user(rid);
     // return(username);
   }

   if (*newp == '4') {
     interactive_addusrgrp(rid);
     // return(username);
   }

   if (*newp == '5') {
     interactive_remusrgrp(rid);
     // return(username);
   }
   
   if (*newp == '6') {
     unbind_internetprovider(rid);
     // return(username);
   }


#ifdef DOCRYPTO
   if (*newp == '9') {   /* Set new password */

     if (dontchange) {
       printf("Sorry, unable to edit since password seems blank already (thus no space for it)\n");
       return(0);
     }

     pl = fmyinput("New Password: ",newp,16);

     if (pl < 1) {
       printf("No change.\n");
       return(0);
     }

     cheap_ascii2uni(newp,newunipw,pl);
   
     make_lanmpw(newp,newlanpw,pl);

     /*   printf("Ucase Lanman: %s\n",newlanpw); */
   
     MD4Init (&context);
     MD4Update (&context, newunipw, pl<<1);
     MD4Final (digest, &context);
     
     if (gverbose) hexprnt("\nNEW MD4 hash    : ",digest,16);
     
     E1((uchar *)newlanpw,   x1, (uchar *)lanman);
     E1((uchar *)newlanpw+7, x1, (uchar *)lanman+8);
     
     if (gverbose) hexprnt("NEW LANMAN hash : ",(unsigned char *)lanman,16);
     
     /* Encrypt the NT md4 password hash as two 8 byte blocks. */
     des_ecb_encrypt((des_cblock *)digest,
		     (des_cblock *)despw, ks1, DES_ENCRYPT);
     des_ecb_encrypt((des_cblock *)(digest+8),
		     (des_cblock *)&despw[8], ks2, DES_ENCRYPT);
     
     des_ecb_encrypt((des_cblock *)lanman,
		     (des_cblock *)newlandes, ks1, DES_ENCRYPT);
     des_ecb_encrypt((des_cblock *)(lanman+8),
		     (des_cblock *)&newlandes[8], ks2, DES_ENCRYPT);
     
     if (gverbose) {
       hexprnt("NEW DES crypt   : ",(unsigned char *)despw,16);
       hexprnt("NEW LANMAN crypt: ",(unsigned char *)newlandes,16);
     }

     /* Reset hash length to 16 if syskey enabled, this will cause
      * a conversion to syskey-hashes upon next boot */
     if (syskeyreset && ntpw_len > 16) { 
       ntpw_len = 16;
       lmpw_len = 16;
       ntpw_offs -= 4;
       *(vp+0xa8) = (unsigned int)(ntpw_offs - 0xcc);
       *(vp + 0xa0) = 16;
       *(vp + 0xac) = 16;
     }
     
     for (i = 0; i < 16; i++) {
       *(vp+ntpw_offs+i) = (unsigned char)despw[i];
       if (lmpw_len >= 16) *(vp+lmpw_offs+i) = (unsigned char)newlandes[i];
     }

     printf("Password changed!\n");


   } /* new password */
#endif /* DOCRYPTO */
   if (*newp == 's' || *newp == 'S') {
     acb = change_accountbits(acb);
   }

   if (*newp == 'e' || *newp == 'E') {
     puts("Enter a new value in the form of 0x2102, followed by a newline:");
     scanf("0x%4xh\n", &acb);
   }

   if (pl == 1 && *newp == '1') {
     /* Setting hash lengths to zero seems to make NT think it is blank
      * However, since we cant cut the previous hash bytes out of the V value
      * due to missing resize-support of values, it may leak about 40 bytes
      * each time we do this.
      */
     v->ntpw_len = 0;
     v->lmpw_len = 0;
     dirty = 1;

     printf("Password cleared!\n");
   }
   
#ifdef SYSKEY
   hexprnt("Pw in buffer: ",(vp+ntpw_offs),16);
   hexprnt("Lm in buffer: ",(vp+lmpw_offs),16);
#endif
   } // Forever...

   return(username);
}


/* Find a username in the SAM registry, then get it's V-value,
 * and feed it to the password changer.
 */

void find_n_change(char *username)
{
  char s[200];
  struct keyval *v;
  int rid = 0;

  if ((H_SAM < 0) || (!username)) return;
  if (*username == '0' && *(username+1) == 'x') sscanf(username,"%i",&rid);
  
  if (!rid) { /* Look up username */
    /* Extract the unnamed value out of the username-key, value is RID  */
    snprintf(s,180,"\\SAM\\Domains\\Account\\Users\\Names\\%s\\@",username);
    rid = get_dword(hive[H_SAM],0,s, TPF_VK_EXACT|TPF_VK_SHORT);
    if (rid == -1) {
      printf("Cannot find user, path is <%s>\n",s);
      return;
    }
  }

  /*
  printf("Username: %s, RID = %d (0x%0x)\n",username,rid,rid);
  */

  /* Now that we have the RID, build the path to, and get the V-value */
  snprintf(s,180,"\\SAM\\Domains\\Account\\Users\\%08X\\V",rid);
  v = get_val2buf(hive[H_SAM], NULL, 0, s, REG_BINARY, TPF_VK_EXACT);
  if (!v) {
    printf("Cannot find users V value <%s>\n",s);
    return;
  }

  if (v->len < 0xcc) {
    printf("Value <%s> is too short (only %d bytes) to be a SAM user V-struct!\n",
	   s, v->len);
  } else {
    change_pw( (char *)&v->data , rid, v->len, 0);
    if (dirty) {
      if (!(put_buf2val(hive[H_SAM], v, 0, s, REG_BINARY, TPF_VK_EXACT))) {
	printf("Failed to write updated <%s> to registry! Password change not completed!\n",s);
      }
    }
  }
  FREE(v);
}

#ifdef SYSKEY
/* Check for presence of syskey and possibly disable it if
 * user wants it.
 * This is tricky, and extremely undocumented!
 * See docs for more info on what's going on when syskey is installed
 */
void handle_syskey(void)
{
  /* void *fdata; */
  struct samkeyf *ff = NULL;
  struct secpoldata *sf = NULL;
  int /* len, */ i, secboot, samfmode, secmode;
  struct keyval *samf, *secpol;
  char *syskeytypes[4] = { "off", "key-in-registry", "enter-passphrase", "key-on-floppy" }; 
  char yn[5];

  #ifdef LSADATA
  struct lsadata *ld = NULL;
  int ldmode;
  struct keyval *lsad;
  #endif

  printf("\n---------------------> SYSKEY CHECK <-----------------------\n");


  if (H_SAM < 0) {
    printf("ERROR: SAM hive not loaded!\n");
    return;
  }
  samf = get_val2buf(hive[H_SAM], NULL, 0, "\\SAM\\Domains\\Account\\F", REG_BINARY, TPF_VK_EXACT);

  if (samf && samf->len > 0x70 ) {
    ff = (struct samkeyf *)&samf->data;
    samfmode = ff->syskeymode;
  } else {
    samfmode = -1;
  }

  secboot = -1;
  if (H_SYS >= 0) {
    secboot = get_dword(hive[H_SYS], 0, "\\ControlSet001\\Control\\Lsa\\SecureBoot", TPF_VK_EXACT );
  }

  secmode = -1;
  if (H_SEC >=0) {
    secpol = get_val2buf(hive[H_SEC], NULL, 0, "\\Policy\\PolSecretEncryptionKey\\@", REG_NONE, TPF_VK_EXACT);
    if (secpol) {     /* Will not be found in NT 4, take care of that */
      sf = (struct secpoldata *)&secpol->data;
      secmode = sf->syskeymode;
    }
  }

#ifdef LSADATA
  if (H_SYS >= 0) {
    lsad = get_val2buf(hive[H_SYS], NULL, 0, "\\ControlSet001\\Control\\Lsa\\Data\\Pattern", REG_BINARY, TPF_VK_EXACT);

    if (lsad && lsad->len >= 0x38) {
      ld = (struct lsadata *)&lsad->data;
      ldmode = ld->syskeymode;
    } else {
      ldmode = -1;
    }
  }
#endif

  printf("SYSTEM   SecureBoot            : %d -> %s\n", secboot,
	 (secboot < 0 || secboot > 3) ? "Not Set (not installed, good!)" : syskeytypes[secboot]);
  printf("SAM      Account\\F             : %d -> %s\n", samfmode,
	 (samfmode < 0 || samfmode > 3) ? "Not Set" : syskeytypes[samfmode]);
  printf("SECURITY PolSecretEncryptionKey: %d -> %s\n", secmode,
	 (secmode < 0 || secmode > 3) ? "Not Set (OK if this is NT4)" : syskeytypes[secmode]);

#ifdef LSADATA
  printf("SYSTEM   LsaData               : %d -> %s\n\n", ldmode,
	 (ldmode < 0 || ldmode > 3) ? "Not Set (strange?)" : syskeytypes[ldmode]);
#endif

  if (secboot != samfmode && secboot != -1) {
    printf("WARNING: Mismatch in syskey settings in SAM and SYSTEM!\n");
    printf("WARNING: It may be dangerous to continue (however, resetting syskey\n");
    printf("         may very well fix the problem)\n");
  }

  if (secboot > 0 || samfmode > 0) {
    printf("\n***************** SYSKEY IS ENABLED! **************\n");
    printf("This installation very likely has the syskey passwordhash-obfuscator installed\n");
    printf("It's currently in mode = %d, %s-mode\n",secboot,
	   (secboot < 0 || secboot > 3) ? "Unknown" : syskeytypes[secboot]);

    if (no_hives < 2) {
      printf("\nSYSTEM (and possibly SECURITY) hives not loaded, unable to disable syskey!\n");
      printf("Please start the program with at least SAM & SYSTEM-hive filenames as arguments!\n\n");
      return;
    }
    printf("SYSKEY is on! However, DO NOT DISABLE IT UNLESS YOU HAVE TO!\n");
    printf("This program can change passwords even if syskey is on, however\n");
    printf("if you have lost the key-floppy or passphrase you can turn it off,\n");
    printf("but please read the docs first!!!\n");
    printf("\n** IF YOU DON'T KNOW WHAT SYSKEY IS YOU DO NOT NEED TO SWITCH IT OFF!**\n");
    printf("NOTE: On WINDOWS 2000 and XP it will not be possible\n");
    printf("to turn it on again! (and other problems may also show..)\n\n");
    printf("NOTE: Disabling syskey will invalidate ALL\n");
    printf("passwords, requiring them to be reset. You should at least reset the\n");
    printf("administrator password using this program, then the rest ought to be\n");
    printf("done from NT.\n");
    printf("\nEXTREME WARNING: Do not try this on Vista or Win 7, it will go into endless re-boots\n\n");

    fmyinput("\nDo you really wish to disable SYSKEY? (y/n) [n] ",yn,2);
    if (*yn == 'y') {
      /* Reset SAM syskey infostruct, fill with zeroes */
      if (ff) { 
	ff->syskeymode = 0;

	for (i = 0; i < 0x3b; i++) {
	  ff->syskeyflags1[i] = 0;
	}

	put_buf2val(hive[H_SAM], samf, 0, "\\SAM\\Domains\\Account\\F", REG_BINARY, TPF_VK_EXACT);

      }
      /* Reset SECURITY infostruct (if any) */
      if (sf) { 
	memset(sf, 0, secpol->len);
	sf->syskeymode = 0;

	put_buf2val(hive[H_SEC], secpol, 0, "\\Policy\\PolSecretEncryptionKey\\@", REG_BINARY, TPF_VK_EXACT);

      }

#if defined(LSADATA) && defined(LSADATA_WRITE)
      if (ld) { 

	ld->syskeymode = 0;

	put_buf2val(hive[H_SYS], lsad, 0, "\\ControlSet001\\Control\\Lsa\\Data\\Pattern", REG_BINARY, TPF_VK_EXACT);

      }
#endif

      /* And SYSTEM SecureBoot parameter */

      put_dword(hive[H_SYS], 0, "\\ControlSet001\\Control\\Lsa\\SecureBoot", TPF_VK_EXACT, 0);

      dirty = 1;
      syskeyreset = 1;
      printf("Updating passwordhash-lengths..\n");
      sam_list_users(hive[H_SAM], 1);
      printf("* SYSKEY RESET!\nNow please set new administrator password!\n");
    } else {

      syskeyreset = 1;
    }
  } else {
    printf("Syskey not installed!\n");
    return;
  }

}
#endif /* SYSKEY */


/* Interactive user edit */
void useredit(void)
{
  char iwho[100];
  int il, admrid;
  int rid = 0;

  printf("\n\n===== chntpw Edit User Info & Passwords ====\n\n");

  if (H_SAM < 0) {
    printf("ERROR: SAM registry file (which contains user data) is not loaded!\n\n");
    return;
  }


  admrid = sam_list_users(hive[H_SAM], 1);

  
  printf("\nPlease enter user number (RID) or 0 to exit: [%x] ", admrid);

  il = fmyinput("",iwho,32);

  if (il == 0) {
    sprintf(iwho,"0x%x",admrid);
    rid = admrid;
  } else {
    sscanf(iwho, "%x", &rid);
    sprintf(iwho,"0x%x",rid);   
  }
  if (!rid) return;

  find_n_change(iwho);
  
}


void recoveryconsole()
{

  int cmd = 0;
  int sec = 0;
  static char *scpath = "\\Microsoft\\Windows NT\\CurrentVersion\\Setup\\RecoveryConsole\\SetCommand";
  static char *slpath = "\\Microsoft\\Windows NT\\CurrentVersion\\Setup\\RecoveryConsole\\SecurityLevel";
  char yn[5];

  if (H_SOF < 0) {
    printf("\nSOFTWARE-hive not loaded, and there's where RecoveryConsole settings are..\n");
    return;
  }

  cmd = get_dword(hive[H_SOF],0,scpath,TPF_VK_EXACT);
  sec = get_dword(hive[H_SOF],0,slpath,TPF_VK_EXACT);

  if (cmd == -1 && sec == -1) {
    printf("\nDid not find registry entries for RecoveryConsole.\n(RecoveryConsole is only in Windows 2000 and XP)\n");
    return;
  }

  printf("\nRecoveryConsole:\n- Extended SET command is \t%s\n", cmd>0 ? "ENABLED (1)" : "DISABLED (0)");
  printf("- Administrator password login: %s\n", sec>0 ? "SKIPPED (1)" : "ENFORCED (0)");

  fmyinput("\nDo you want to change it? (y/n) [n] ",yn,2);
  if (*yn == 'y') {
    cmd ^= 1;
    sec ^= 1;
    if (!put_dword(hive[0], 0, scpath, TPF_VK_EXACT, cmd)) printf("Update of SET level failed registry edit\n");
    if (!put_dword(hive[0], 0, slpath, TPF_VK_EXACT, sec)) printf("Update of login level failed registry edit\n");
    printf("Done!\n");
  }

}



void listgroups(void)
{
  char yn[8];
  int il;
  int members = 0;

  il = fmyinput("Also list group members? [n] ", yn, 2);

  if (il && (yn[0] == 'y' || yn[0] == 'Y')) members = 1;

  sam_list_groups(hive[H_SAM], members, 1);

}



/* Interactive menu system */

void interactive(void)
{
  int il;
  char inbuf[20];

  while(1) {
    printf("\n\n<>========<> chntpw Main Interactive Menu <>========<>\n\n"
	   "Loaded hives:");
    for (il = 0; il < no_hives; il++) {
      printf(" <%s>",hive[il]->filename);
    }

    printf("\n\n");

    /* Make menu selection depending on what is loaded
       but it is still possible to select even if not shown */

    if (H_SAM >= 0) {
      printf("  1 - Edit user data and passwords\n");
      printf("  2 - List groups\n");
    }
    if (H_SOF >= 0) {
      printf("  3 - RecoveryConsole settings\n");
      printf("  4 - Show product key (DigitalProductID)\n");
    }
#if defined(SYSKEY) && defined(SYSKEY_SHOW)
    if (H_SAM >= 0 && H_SYS >= 0 && H_SEC >= 0) {
      printf("  8 - Syskey status & change (dangerous, unnecessary)\n");
    }
#endif

    printf("      - - -\n"
	   "  9 - Registry editor, now with full write support!\n"
	   "  q - Quit (you will be asked if there is something to save)\n"
	   "\n\n");

    il = fmyinput("What to do? [1] -> ", inbuf, 10);
    
    if (!il) useredit();
    if (il) {
      switch(inbuf[0]) {
      case '1': useredit(); break;
      case '2': listgroups(); break;
      case '3': recoveryconsole(); break;
      case '4': cat_dpi(hive[H_SOF],0,"\\Microsoft\\Windows NT\\CurrentVersion\\DigitalProductId"); break;
#ifdef SYSKEY
      case '8': handle_syskey(); break;
#endif
      case '9': regedit_interactive(hive, no_hives); break;
      case 'q': return; break;
      }
    }
  }
}

  
void usage(void) {
   printf("chntpw: change password of a user in a Windows SAM file,\n"
	  "or invoke registry editor. Should handle both 32 and 64 bit windows and\n"
	  "all version from NT3.x to Win8.1\n"
	  "chntpw [OPTIONS] <samfile> [systemfile] [securityfile] [otherreghive] [...]\n"
	  " -h          This message\n"
	  " -u <user>   Username or RID (0x3e9 for example) to interactively edit\n"
	  " -l          list all users in SAM file and exit\n"
	  " -i          Interactive Menu system\n"
  //          " -f          Interactively edit first admin user\n"
	  " -e          Registry editor. Now with full write support!\n"
	  " -d          Enter buffer debugger instead (hex editor), \n"
          " -v          Be a little more verbose (for debuging)\n"
	  " -L          For scripts, write names of changed files to /tmp/changed\n"
	  " -N          No allocation mode. Only same length overwrites possible (very safe mode)\n"
	  " -E          No expand mode, do not expand hive file (safe mode)\n"
	  
	  "\nUsernames can be given as name or RID (in hex with 0x first)\n"
          "\nSee readme file on how to get to the registry files, and what they are.\n"
          "Source/binary freely distributable under GPL v2 license. See README for details.\n"
          "NOTE: This program is somewhat hackish! You are on your own!\n"
	  );
}


int main(int argc, char **argv)
{
  
  int dodebug = 0, list = 0, inter = 0,edit = 0,il,d = 0, dd = 0, logchange = 0;
  int mode = HMODE_INFO;
  extern int /* opterr, */ optind;
  extern char* optarg;
  char *filename,c;
  char *who = "Administrator";
  char iwho[100];
  FILE *ch;     /* Write out names of touched files to this */
   
  char *options = "LENidehflvu:";
  
  while((c=getopt(argc,argv,options)) > 0) {
    switch(c) {
    case 'd': dodebug = 1; break;
    case 'e': edit = 1; break;
    case 'L': logchange = 1; break;
    case 'N': mode |= HMODE_NOALLOC; break;
    case 'E': mode |= HMODE_NOEXPAND; break;
    case 'l': list = 1; break;
    case 'v': mode |= HMODE_VERBOSE; gverbose = 1; break;
    case 'i': inter = 1; break;
    case 'u': who = optarg; break;
    case 'h': usage(); exit(0); break;
    default: usage(); exit(1); break;
    }
  }

  printf("%s\n",chntpw_version);

  filename=argv[optind];
  if (!filename || !*filename) {
    usage(); exit(1);
  }
  do {
    if (!(hive[no_hives] = openHive(filename,
				    HMODE_RW|mode))) {
      fprintf(stderr,"%s: Unable to open/read a hive, exiting..\n",argv[0]);
      exit(1);
    }
    switch(hive[no_hives]->type) {
    case HTYPE_SAM:      H_SAM = no_hives; break;
    case HTYPE_SOFTWARE: H_SOF = no_hives; break;
    case HTYPE_SYSTEM:   H_SYS = no_hives; break;
    case HTYPE_SECURITY: H_SEC = no_hives; break;
    }
    no_hives++;
    filename = argv[optind+no_hives];
  } while (filename && *filename && no_hives < MAX_HIVES);
  
  
  if (dodebug) {
    debugit(hive[0]->buffer,hive[0]->size);
  } else {
    if (H_SAM != -1) max_sam_lock = sam_get_lockoutinfo(hive[H_SAM], 0);
    if (inter) {
      interactive();
    } else if (edit) {
      regedit_interactive(hive, no_hives);
    } else if (list) {
      sam_list_users(hive[H_SAM], 1);
    } else if (who) {
      find_n_change(who);
    }
  }
  

  if (list != 1) {
    printf("\nHives that have changed:\n #  Name\n");
    for (il = 0; il < no_hives; il++) {
      if (hive[il]->state & HMODE_DIRTY) {
	if (!logchange) printf("%2d  <%s>",il,hive[il]->filename);
	if (hive[il]->state & HMODE_DIDEXPAND) printf(" WARNING: File was expanded! Expermental! Use at own risk!\n");
	printf("\n");
	
	d = 1;
      }
    }
    if (d) {
      /* Only prompt user if logging of changed files has not been set */
      /* Thus we assume confirmations are done externally if they ask for a list of changes */
      if (!logchange) fmyinput("Write hive files? (y/n) [n] : ",iwho,3);
      if (*iwho == 'y' || logchange) {
	if (logchange) {
	  ch = fopen("/tmp/changed","w");
	}
	for (il = 0; il < no_hives; il++) {
	  if (hive[il]->state & HMODE_DIRTY) {
	    printf("%2d  <%s> - ",il,hive[il]->filename);
	    if (!writeHive(hive[il])) {
	      printf("OK");
	      if (hive[il]->state & HMODE_DIDEXPAND) printf(" WARNING: File was expanded! Expermental! Use at own risk!\n");
	      printf("\n");
	      if (logchange) fprintf(ch,"%s ",hive[il]->filename);
	      dd = 2;
	    }
	  }
	}
	if (logchange) {
	  fprintf(ch,"\n");
	  fclose(ch);
	}
      } else {
	printf("Not written!\n\n");
      }
    } else {
      printf("None!\n\n");
    }
  } /* list only check */
  return(dd);
}
