/*
 * conversion between client encoding and server internal encoding
 * (currently mule internal code (mic) is used)
 * Tatsuo Ishii
 * WIN1250 client encoding support contributed by Pavel Behal
 *
 * $Id$
 *
 *
 */

#include <stdio.h>
#include <string.h>

#include "mb/pg_wchar.h"

/*
 * SJIS alternative code.
 * this code is used if a mapping EUC -> SJIS is not defined.
 */
#define PGSJISALTCODE 0x81ac
#define PGEUCALTCODE 0xa2ae

/*
 * conversion table between SJIS UDC (IBM kanji) and EUC_JP
 */
static struct
{
	int			sjis;			/* SJIS UDC (IBM kanji) */
	int			euc;			/* EUC_JP */
}			ibmkanji[] =

{
	{
		0xfa40, 0x8ff3f3
	},
	{
		0xfa41, 0x8ff3f4
	},
	{
		0xfa42, 0x8ff3f5
	},
	{
		0xfa43, 0x8ff3f6
	},
	{
		0xfa44, 0x8ff3f7
	},
	{
		0xfa45, 0x8ff3f8
	},
	{
		0xfa46, 0x8ff3f9
	},
	{
		0xfa47, 0x8ff3fa
	},
	{
		0xfa48, 0x8ff3fb
	},
	{
		0xfa49, 0x8ff3fc
	},
	{
		0xfa4a, 0x8ff3fd
	},
	{
		0xfa4b, 0x8ff3fe
	},
	{
		0xfa4c, 0x8ff4a1
	},
	{
		0xfa4d, 0x8ff4a2
	},
	{
		0xfa4e, 0x8ff4a3
	},
	{
		0xfa4f, 0x8ff4a4
	},
	{
		0xfa50, 0x8ff4a5
	},
	{
		0xfa51, 0x8ff4a6
	},
	{
		0xfa52, 0x8ff4a7
	},
	{
		0xfa53, 0x8ff4a8
	},
	{
		0xfa54, 0xa2cc
	},
	{
		0xfa55, 0x8fa2c3
	},
	{
		0xfa56, 0x8ff4a9
	},
	{
		0xfa57, 0x8ff4aa
	},
	{
		0xfa58, 0x8ff4ab
	},
	{
		0xfa59, 0x8ff4ac
	},
	{
		0xfa5a, 0x8ff4ad
	},
	{
		0xfa5b, 0xa2e8
	},
	{
		0xfa5c, 0x8fd4e3
	},
	{
		0xfa5d, 0x8fdcdf
	},
	{
		0xfa5e, 0x8fe4e9
	},
	{
		0xfa5f, 0x8fe3f8
	},
	{
		0xfa60, 0x8fd9a1
	},
	{
		0xfa61, 0x8fb1bb
	},
	{
		0xfa62, 0x8ff4ae
	},
	{
		0xfa63, 0x8fc2ad
	},
	{
		0xfa64, 0x8fc3fc
	},
	{
		0xfa65, 0x8fe4d0
	},
	{
		0xfa66, 0x8fc2bf
	},
	{
		0xfa67, 0x8fbcf4
	},
	{
		0xfa68, 0x8fb0a9
	},
	{
		0xfa69, 0x8fb0c8
	},
	{
		0xfa6a, 0x8ff4af
	},
	{
		0xfa6b, 0x8fb0d2
	},
	{
		0xfa6c, 0x8fb0d4
	},
	{
		0xfa6d, 0x8fb0e3
	},
	{
		0xfa6e, 0x8fb0ee
	},
	{
		0xfa6f, 0x8fb1a7
	},
	{
		0xfa70, 0x8fb1a3
	},
	{
		0xfa71, 0x8fb1ac
	},
	{
		0xfa72, 0x8fb1a9
	},
	{
		0xfa73, 0x8fb1be
	},
	{
		0xfa74, 0x8fb1df
	},
	{
		0xfa75, 0x8fb1d8
	},
	{
		0xfa76, 0x8fb1c8
	},
	{
		0xfa77, 0x8fb1d7
	},
	{
		0xfa78, 0x8fb1e3
	},
	{
		0xfa79, 0x8fb1f4
	},
	{
		0xfa7a, 0x8fb1e1
	},
	{
		0xfa7b, 0x8fb2a3
	},
	{
		0xfa7c, 0x8ff4b0
	},
	{
		0xfa7d, 0x8fb2bb
	},
	{
		0xfa7e, 0x8fb2e6
	},
	{
		0xfa80, 0x8fb2ed
	},
	{
		0xfa81, 0x8fb2f5
	},
	{
		0xfa82, 0x8fb2fc
	},
	{
		0xfa83, 0x8ff4b1
	},
	{
		0xfa84, 0x8fb3b5
	},
	{
		0xfa85, 0x8fb3d8
	},
	{
		0xfa86, 0x8fb3db
	},
	{
		0xfa87, 0x8fb3e5
	},
	{
		0xfa88, 0x8fb3ee
	},
	{
		0xfa89, 0x8fb3fb
	},
	{
		0xfa8a, 0x8ff4b2
	},
	{
		0xfa8b, 0x8ff4b3
	},
	{
		0xfa8c, 0x8fb4c0
	},
	{
		0xfa8d, 0x8fb4c7
	},
	{
		0xfa8e, 0x8fb4d0
	},
	{
		0xfa8f, 0x8fb4de
	},
	{
		0xfa90, 0x8ff4b4
	},
	{
		0xfa91, 0x8fb5aa
	},
	{
		0xfa92, 0x8ff4b5
	},
	{
		0xfa93, 0x8fb5af
	},
	{
		0xfa94, 0x8fb5c4
	},
	{
		0xfa95, 0x8fb5e8
	},
	{
		0xfa96, 0x8ff4b6
	},
	{
		0xfa97, 0x8fb7c2
	},
	{
		0xfa98, 0x8fb7e4
	},
	{
		0xfa99, 0x8fb7e8
	},
	{
		0xfa9a, 0x8fb7e7
	},
	{
		0xfa9b, 0x8ff4b7
	},
	{
		0xfa9c, 0x8ff4b8
	},
	{
		0xfa9d, 0x8ff4b9
	},
	{
		0xfa9e, 0x8fb8ce
	},
	{
		0xfa9f, 0x8fb8e1
	},
	{
		0xfaa0, 0x8fb8f5
	},
	{
		0xfaa1, 0x8fb8f7
	},
	{
		0xfaa2, 0x8fb8f8
	},
	{
		0xfaa3, 0x8fb8fc
	},
	{
		0xfaa4, 0x8fb9af
	},
	{
		0xfaa5, 0x8fb9b7
	},
	{
		0xfaa6, 0x8fbabe
	},
	{
		0xfaa7, 0x8fbadb
	},
	{
		0xfaa8, 0x8fcdaa
	},
	{
		0xfaa9, 0x8fbae1
	},
	{
		0xfaaa, 0x8ff4ba
	},
	{
		0xfaab, 0x8fbaeb
	},
	{
		0xfaac, 0x8fbbb3
	},
	{
		0xfaad, 0x8fbbb8
	},
	{
		0xfaae, 0x8ff4bb
	},
	{
		0xfaaf, 0x8fbbca
	},
	{
		0xfab0, 0x8ff4bc
	},
	{
		0xfab1, 0x8ff4bd
	},
	{
		0xfab2, 0x8fbbd0
	},
	{
		0xfab3, 0x8fbbde
	},
	{
		0xfab4, 0x8fbbf4
	},
	{
		0xfab5, 0x8fbbf5
	},
	{
		0xfab6, 0x8fbbf9
	},
	{
		0xfab7, 0x8fbce4
	},
	{
		0xfab8, 0x8fbced
	},
	{
		0xfab9, 0x8fbcfe
	},
	{
		0xfaba, 0x8ff4be
	},
	{
		0xfabb, 0x8fbdc2
	},
	{
		0xfabc, 0x8fbde7
	},
	{
		0xfabd, 0x8ff4bf
	},
	{
		0xfabe, 0x8fbdf0
	},
	{
		0xfabf, 0x8fbeb0
	},
	{
		0xfac0, 0x8fbeac
	},
	{
		0xfac1, 0x8ff4c0
	},
	{
		0xfac2, 0x8fbeb3
	},
	{
		0xfac3, 0x8fbebd
	},
	{
		0xfac4, 0x8fbecd
	},
	{
		0xfac5, 0x8fbec9
	},
	{
		0xfac6, 0x8fbee4
	},
	{
		0xfac7, 0x8fbfa8
	},
	{
		0xfac8, 0x8fbfc9
	},
	{
		0xfac9, 0x8fc0c4
	},
	{
		0xfaca, 0x8fc0e4
	},
	{
		0xfacb, 0x8fc0f4
	},
	{
		0xfacc, 0x8fc1a6
	},
	{
		0xfacd, 0x8ff4c1
	},
	{
		0xface, 0x8fc1f5
	},
	{
		0xfacf, 0x8fc1fc
	},
	{
		0xfad0, 0x8ff4c2
	},
	{
		0xfad1, 0x8fc1f8
	},
	{
		0xfad2, 0x8fc2ab
	},
	{
		0xfad3, 0x8fc2a1
	},
	{
		0xfad4, 0x8fc2a5
	},
	{
		0xfad5, 0x8ff4c3
	},
	{
		0xfad6, 0x8fc2b8
	},
	{
		0xfad7, 0x8fc2ba
	},
	{
		0xfad8, 0x8ff4c4
	},
	{
		0xfad9, 0x8fc2c4
	},
	{
		0xfada, 0x8fc2d2
	},
	{
		0xfadb, 0x8fc2d7
	},
	{
		0xfadc, 0x8fc2db
	},
	{
		0xfadd, 0x8fc2de
	},
	{
		0xfade, 0x8fc2ed
	},
	{
		0xfadf, 0x8fc2f0
	},
	{
		0xfae0, 0x8ff4c5
	},
	{
		0xfae1, 0x8fc3a1
	},
	{
		0xfae2, 0x8fc3b5
	},
	{
		0xfae3, 0x8fc3c9
	},
	{
		0xfae4, 0x8fc3b9
	},
	{
		0xfae5, 0x8ff4c6
	},
	{
		0xfae6, 0x8fc3d8
	},
	{
		0xfae7, 0x8fc3fe
	},
	{
		0xfae8, 0x8ff4c7
	},
	{
		0xfae9, 0x8fc4cc
	},
	{
		0xfaea, 0x8ff4c8
	},
	{
		0xfaeb, 0x8fc4d9
	},
	{
		0xfaec, 0x8fc4ea
	},
	{
		0xfaed, 0x8fc4fd
	},
	{
		0xfaee, 0x8ff4c9
	},
	{
		0xfaef, 0x8fc5a7
	},
	{
		0xfaf0, 0x8fc5b5
	},
	{
		0xfaf1, 0x8fc5b6
	},
	{
		0xfaf2, 0x8ff4ca
	},
	{
		0xfaf3, 0x8fc5d5
	},
	{
		0xfaf4, 0x8fc6b8
	},
	{
		0xfaf5, 0x8fc6d7
	},
	{
		0xfaf6, 0x8fc6e0
	},
	{
		0xfaf7, 0x8fc6ea
	},
	{
		0xfaf8, 0x8fc6e3
	},
	{
		0xfaf9, 0x8fc7a1
	},
	{
		0xfafa, 0x8fc7ab
	},
	{
		0xfafb, 0x8fc7c7
	},
	{
		0xfafc, 0x8fc7c3
	},
	{
		0xfb40, 0x8fc7cb
	},
	{
		0xfb41, 0x8fc7cf
	},
	{
		0xfb42, 0x8fc7d9
	},
	{
		0xfb43, 0x8ff4cb
	},
	{
		0xfb44, 0x8ff4cc
	},
	{
		0xfb45, 0x8fc7e6
	},
	{
		0xfb46, 0x8fc7ee
	},
	{
		0xfb47, 0x8fc7fc
	},
	{
		0xfb48, 0x8fc7eb
	},
	{
		0xfb49, 0x8fc7f0
	},
	{
		0xfb4a, 0x8fc8b1
	},
	{
		0xfb4b, 0x8fc8e5
	},
	{
		0xfb4c, 0x8fc8f8
	},
	{
		0xfb4d, 0x8fc9a6
	},
	{
		0xfb4e, 0x8fc9ab
	},
	{
		0xfb4f, 0x8fc9ad
	},
	{
		0xfb50, 0x8ff4cd
	},
	{
		0xfb51, 0x8fc9ca
	},
	{
		0xfb52, 0x8fc9d3
	},
	{
		0xfb53, 0x8fc9e9
	},
	{
		0xfb54, 0x8fc9e3
	},
	{
		0xfb55, 0x8fc9fc
	},
	{
		0xfb56, 0x8fc9f4
	},
	{
		0xfb57, 0x8fc9f5
	},
	{
		0xfb58, 0x8ff4ce
	},
	{
		0xfb59, 0x8fcab3
	},
	{
		0xfb5a, 0x8fcabd
	},
	{
		0xfb5b, 0x8fcaef
	},
	{
		0xfb5c, 0x8fcaf1
	},
	{
		0xfb5d, 0x8fcbae
	},
	{
		0xfb5e, 0x8ff4cf
	},
	{
		0xfb5f, 0x8fcbca
	},
	{
		0xfb60, 0x8fcbe6
	},
	{
		0xfb61, 0x8fcbea
	},
	{
		0xfb62, 0x8fcbf0
	},
	{
		0xfb63, 0x8fcbf4
	},
	{
		0xfb64, 0x8fcbee
	},
	{
		0xfb65, 0x8fcca5
	},
	{
		0xfb66, 0x8fcbf9
	},
	{
		0xfb67, 0x8fccab
	},
	{
		0xfb68, 0x8fccae
	},
	{
		0xfb69, 0x8fccad
	},
	{
		0xfb6a, 0x8fccb2
	},
	{
		0xfb6b, 0x8fccc2
	},
	{
		0xfb6c, 0x8fccd0
	},
	{
		0xfb6d, 0x8fccd9
	},
	{
		0xfb6e, 0x8ff4d0
	},
	{
		0xfb6f, 0x8fcdbb
	},
	{
		0xfb70, 0x8ff4d1
	},
	{
		0xfb71, 0x8fcebb
	},
	{
		0xfb72, 0x8ff4d2
	},
	{
		0xfb73, 0x8fceba
	},
	{
		0xfb74, 0x8fcec3
	},
	{
		0xfb75, 0x8ff4d3
	},
	{
		0xfb76, 0x8fcef2
	},
	{
		0xfb77, 0x8fb3dd
	},
	{
		0xfb78, 0x8fcfd5
	},
	{
		0xfb79, 0x8fcfe2
	},
	{
		0xfb7a, 0x8fcfe9
	},
	{
		0xfb7b, 0x8fcfed
	},
	{
		0xfb7c, 0x8ff4d4
	},
	{
		0xfb7d, 0x8ff4d5
	},
	{
		0xfb7e, 0x8ff4d6
	},
	{
		0xfb80, 0x8ff4d7
	},
	{
		0xfb81, 0x8fd0e5
	},
	{
		0xfb82, 0x8ff4d8
	},
	{
		0xfb83, 0x8fd0e9
	},
	{
		0xfb84, 0x8fd1e8
	},
	{
		0xfb85, 0x8ff4d9
	},
	{
		0xfb86, 0x8ff4da
	},
	{
		0xfb87, 0x8fd1ec
	},
	{
		0xfb88, 0x8fd2bb
	},
	{
		0xfb89, 0x8ff4db
	},
	{
		0xfb8a, 0x8fd3e1
	},
	{
		0xfb8b, 0x8fd3e8
	},
	{
		0xfb8c, 0x8fd4a7
	},
	{
		0xfb8d, 0x8ff4dc
	},
	{
		0xfb8e, 0x8ff4dd
	},
	{
		0xfb8f, 0x8fd4d4
	},
	{
		0xfb90, 0x8fd4f2
	},
	{
		0xfb91, 0x8fd5ae
	},
	{
		0xfb92, 0x8ff4de
	},
	{
		0xfb93, 0x8fd7de
	},
	{
		0xfb94, 0x8ff4df
	},
	{
		0xfb95, 0x8fd8a2
	},
	{
		0xfb96, 0x8fd8b7
	},
	{
		0xfb97, 0x8fd8c1
	},
	{
		0xfb98, 0x8fd8d1
	},
	{
		0xfb99, 0x8fd8f4
	},
	{
		0xfb9a, 0x8fd9c6
	},
	{
		0xfb9b, 0x8fd9c8
	},
	{
		0xfb9c, 0x8fd9d1
	},
	{
		0xfb9d, 0x8ff4e0
	},
	{
		0xfb9e, 0x8ff4e1
	},
	{
		0xfb9f, 0x8ff4e2
	},
	{
		0xfba0, 0x8ff4e3
	},
	{
		0xfba1, 0x8ff4e4
	},
	{
		0xfba2, 0x8fdcd3
	},
	{
		0xfba3, 0x8fddc8
	},
	{
		0xfba4, 0x8fddd4
	},
	{
		0xfba5, 0x8fddea
	},
	{
		0xfba6, 0x8fddfa
	},
	{
		0xfba7, 0x8fdea4
	},
	{
		0xfba8, 0x8fdeb0
	},
	{
		0xfba9, 0x8ff4e5
	},
	{
		0xfbaa, 0x8fdeb5
	},
	{
		0xfbab, 0x8fdecb
	},
	{
		0xfbac, 0x8ff4e6
	},
	{
		0xfbad, 0x8fdfb9
	},
	{
		0xfbae, 0x8ff4e7
	},
	{
		0xfbaf, 0x8fdfc3
	},
	{
		0xfbb0, 0x8ff4e8
	},
	{
		0xfbb1, 0x8ff4e9
	},
	{
		0xfbb2, 0x8fe0d9
	},
	{
		0xfbb3, 0x8ff4ea
	},
	{
		0xfbb4, 0x8ff4eb
	},
	{
		0xfbb5, 0x8fe1e2
	},
	{
		0xfbb6, 0x8ff4ec
	},
	{
		0xfbb7, 0x8ff4ed
	},
	{
		0xfbb8, 0x8ff4ee
	},
	{
		0xfbb9, 0x8fe2c7
	},
	{
		0xfbba, 0x8fe3a8
	},
	{
		0xfbbb, 0x8fe3a6
	},
	{
		0xfbbc, 0x8fe3a9
	},
	{
		0xfbbd, 0x8fe3af
	},
	{
		0xfbbe, 0x8fe3b0
	},
	{
		0xfbbf, 0x8fe3aa
	},
	{
		0xfbc0, 0x8fe3ab
	},
	{
		0xfbc1, 0x8fe3bc
	},
	{
		0xfbc2, 0x8fe3c1
	},
	{
		0xfbc3, 0x8fe3bf
	},
	{
		0xfbc4, 0x8fe3d5
	},
	{
		0xfbc5, 0x8fe3d8
	},
	{
		0xfbc6, 0x8fe3d6
	},
	{
		0xfbc7, 0x8fe3df
	},
	{
		0xfbc8, 0x8fe3e3
	},
	{
		0xfbc9, 0x8fe3e1
	},
	{
		0xfbca, 0x8fe3d4
	},
	{
		0xfbcb, 0x8fe3e9
	},
	{
		0xfbcc, 0x8fe4a6
	},
	{
		0xfbcd, 0x8fe3f1
	},
	{
		0xfbce, 0x8fe3f2
	},
	{
		0xfbcf, 0x8fe4cb
	},
	{
		0xfbd0, 0x8fe4c1
	},
	{
		0xfbd1, 0x8fe4c3
	},
	{
		0xfbd2, 0x8fe4be
	},
	{
		0xfbd3, 0x8ff4ef
	},
	{
		0xfbd4, 0x8fe4c0
	},
	{
		0xfbd5, 0x8fe4c7
	},
	{
		0xfbd6, 0x8fe4bf
	},
	{
		0xfbd7, 0x8fe4e0
	},
	{
		0xfbd8, 0x8fe4de
	},
	{
		0xfbd9, 0x8fe4d1
	},
	{
		0xfbda, 0x8ff4f0
	},
	{
		0xfbdb, 0x8fe4dc
	},
	{
		0xfbdc, 0x8fe4d2
	},
	{
		0xfbdd, 0x8fe4db
	},
	{
		0xfbde, 0x8fe4d4
	},
	{
		0xfbdf, 0x8fe4fa
	},
	{
		0xfbe0, 0x8fe4ef
	},
	{
		0xfbe1, 0x8fe5b3
	},
	{
		0xfbe2, 0x8fe5bf
	},
	{
		0xfbe3, 0x8fe5c9
	},
	{
		0xfbe4, 0x8fe5d0
	},
	{
		0xfbe5, 0x8fe5e2
	},
	{
		0xfbe6, 0x8fe5ea
	},
	{
		0xfbe7, 0x8fe5eb
	},
	{
		0xfbe8, 0x8ff4f1
	},
	{
		0xfbe9, 0x8ff4f2
	},
	{
		0xfbea, 0x8ff4f3
	},
	{
		0xfbeb, 0x8fe6e8
	},
	{
		0xfbec, 0x8fe6ef
	},
	{
		0xfbed, 0x8fe7ac
	},
	{
		0xfbee, 0x8ff4f4
	},
	{
		0xfbef, 0x8fe7ae
	},
	{
		0xfbf0, 0x8ff4f5
	},
	{
		0xfbf1, 0x8fe7b1
	},
	{
		0xfbf2, 0x8ff4f6
	},
	{
		0xfbf3, 0x8fe7b2
	},
	{
		0xfbf4, 0x8fe8b1
	},
	{
		0xfbf5, 0x8fe8b6
	},
	{
		0xfbf6, 0x8ff4f7
	},
	{
		0xfbf7, 0x8ff4f8
	},
	{
		0xfbf8, 0x8fe8dd
	},
	{
		0xfbf9, 0x8ff4f9
	},
	{
		0xfbfa, 0x8ff4fa
	},
	{
		0xfbfb, 0x8fe9d1
	},
	{
		0xfbfc, 0x8ff4fb
	},
	{
		0xfc40, 0x8fe9ed
	},
	{
		0xfc41, 0x8feacd
	},
	{
		0xfc42, 0x8ff4fc
	},
	{
		0xfc43, 0x8feadb
	},
	{
		0xfc44, 0x8feae6
	},
	{
		0xfc45, 0x8feaea
	},
	{
		0xfc46, 0x8feba5
	},
	{
		0xfc47, 0x8febfb
	},
	{
		0xfc48, 0x8febfa
	},
	{
		0xfc49, 0x8ff4fd
	},
	{
		0xfc4a, 0x8fecd6
	},
	{
		0xfc4b, 0x8ff4fe
	},
	{
		0xffff, 0xffff
	}							/* stop code */
};

/*
 * convert bogus chars that cannot be represented in the current
encoding
 * system.
 */
static void
printBogusChar(unsigned char **mic, unsigned char **p)
{
	char		strbuf[16];
	int			l = pg_mic_mblen(*mic);

	*(*p)++ = '(';
	while (l--)
	{
		sprintf(strbuf, "%02x", *(*mic)++);
		*(*p)++ = strbuf[0];
		*(*p)++ = strbuf[1];
	}
	*(*p)++ = ')';
}

/*
 * SJIS ---> MIC
 */
static void
sjis2mic(unsigned char *sjis, unsigned char *p, int len)
{
	int			c1,
				c2,
				k;

	while (len > 0 && (c1 = *sjis++))
	{
		if (c1 >= 0xa1 && c1 <= 0xdf)
		{
			/* JIS X0201 (1 byte kana) */
			len--;
			*p++ = LC_JISX0201K;
			*p++ = c1;
		}
		else if (c1 > 0x7f)
		{

			/*
			 * JIS X0208, X0212, user defined extended characters
			 */
			c2 = *sjis++;
			k = (c1 << 8) + c2;

			if (k < 0xeafc)
			{
				/* JIS X0208 */
				len -= 2;
				*p++ = LC_JISX0208;
				*p++ = ((c1 & 0x3f) << 1) + 0x9f + (c2 > 0x9e);
				*p++ = c2 + ((c2 > 0x9e) ? 2 : 0x60) + (c2 < 0x80);
			}
			else if (k >= 0xed40 && k < 0xf040)
			{
				/* NEC sentei IBM kanji */
				*p++ = LC_JISX0208;
				*p++ = PGEUCALTCODE >> 8;
				*p++ = PGEUCALTCODE & 0xff;
			}
			else if (k >= 0xf040 && k < 0xf540)
			{

				/*
				 * UDC1 mapping to X0208 85 ku - 94 ku JIS code 0x7521 -
				 * 0x7e7e EUC 0xf5a1 - 0xfefe
				 */
				len -= 2;
				*p++ = LC_JISX0208;
				c1 -= 0x6f;
				*p++ = ((c1 & 0x3f) << 1) + 0xf3 + (c2 > 0x9e);
				*p++ = c2 + ((c2 > 0x9e) ? 2 : 0x60) + (c2 < 0x80);
			}
			else if (k >= 0xf540 && k < 0xfa40)
			{

				/*
				 * UDC2 mapping to X0212 85 ku - 94 ku JIS code 0x7521 -
				 * 0x7e7e EUC 0x8ff5a1 - 0x8ffefe
				 */
				len -= 2;
				*p++ = LC_JISX0212;
				c1 -= 0x74;
				*p++ = ((c1 & 0x3f) << 1) + 0xf3 + (c2 > 0x9e);
				*p++ = c2 + ((c2 > 0x9e) ? 2 : 0x60) + (c2 < 0x80);
			}
			else if (k >= 0xfa40)
			{

				/*
				 * mapping IBM kanji to X0208 and X0212
				 *
				 */
				int			i,
							k2;

				len -= 2;
				for (i = 0;; i++)
				{
					k2 = ibmkanji[i].sjis;
					if (k2 == 0xffff)
						break;
					if (k2 == k)
					{
						k = ibmkanji[i].euc;
						if (k >= 0x8f0000)
						{
							*p++ = LC_JISX0212;
							*p++ = 0x80 | ((k & 0xff00) >> 8);
							*p++ = 0x80 | (k & 0xff);
						}
						else
						{
							*p++ = LC_JISX0208;
							*p++ = 0x80 | (k >> 8);
							*p++ = 0x80 | (k & 0xff);
						}
					}
				}
			}
		}
		else
		{						/* should be ASCII */
			len--;
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> SJIS
 */
static void
mic2sjis(unsigned char *mic, unsigned char *p, int len)
{
	int			c1,
				c2,
				k;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == LC_JISX0201K)
			*p++ = *mic++;
		else if (c1 == LC_JISX0208)
		{
			c1 = *mic++;
			c2 = *mic++;
			k = (c1 << 8) | (c2 & 0xff);
			if (k >= 0xf5a1)
			{
				/* UDC1 */
				c1 -= 0x54;
				*p++ = ((c1 - 0xa1) >> 1) + ((c1 < 0xdf) ? 0x81 : 0xc1) + 0x6f;
			}
			else
				*p++ = ((c1 - 0xa1) >> 1) + ((c1 < 0xdf) ? 0x81 : 0xc1);
			*p++ = c2 - ((c1 & 1) ? ((c2 < 0xe0) ? 0x61 : 0x60) : 2);
		}
		else if (c1 == LC_JISX0212)
		{
			int			i,
						k2;

			c1 = *mic++;
			c2 = *mic++;
			k = c1 << 8 | c2;
			if (k >= 0xf5a1)
			{
				/* UDC2 */
				c1 -= 0x54;
				*p++ = ((c1 - 0xa1) >> 1) + ((c1 < 0xdf) ? 0x81 : 0xc1) + 0x74;
				*p++ = c2 - ((c1 & 1) ? ((c2 < 0xe0) ? 0x61 : 0x60) : 2);
			}
			else
			{
				/* IBM kanji */
				for (i = 0;; i++)
				{
					k2 = ibmkanji[i].euc & 0xffff;
					if (k2 == 0xffff)
					{
						*p++ = PGSJISALTCODE >> 8;
						*p++ = PGSJISALTCODE & 0xff;
						break;
					}
					if (k2 == k)
					{
						k = ibmkanji[i].sjis;
						*p++ = k >> 8;
						*p++ = k & 0xff;
						break;
					}
				}
			}
		}
		else if (c1 > 0x7f)
		{
			/* cannot convert to SJIS! */
			*p++ = PGSJISALTCODE >> 8;
			*p++ = PGSJISALTCODE & 0xff;
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * EUC_JP ---> MIC
 */
static void
euc_jp2mic(unsigned char *euc, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *euc++))
	{
		if (c1 == SS2)
		{						/* 1 byte kana? */
			len -= 2;
			*p++ = LC_JISX0201K;
			*p++ = *euc++;
		}
		else if (c1 == SS3)
		{						/* JIS X0212 kanji? */
			len -= 3;
			*p++ = LC_JISX0212;
			*p++ = *euc++;
			*p++ = *euc++;
		}
		else if (c1 & 0x80)
		{						/* kanji? */
			len -= 2;
			*p++ = LC_JISX0208;
			*p++ = c1;
			*p++ = *euc++;
		}
		else
		{						/* should be ASCII */
			len--;
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> EUC_JP
 */
static void
mic2euc_jp(unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == LC_JISX0201K)
		{
			*p++ = SS2;
			*p++ = *mic++;
		}
		else if (c1 == LC_JISX0212)
		{
			*p++ = SS3;
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 == LC_JISX0208)
		{
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 > 0x7f)
		{						/* cannot convert to EUC_JP! */
			mic--;
			printBogusChar(&mic, &p);
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * EUC_KR ---> MIC
 */
static void
euc_kr2mic(unsigned char *euc, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *euc++))
	{
		if (c1 & 0x80)
		{
			len -= 2;
			*p++ = LC_KS5601;
			*p++ = c1;
			*p++ = *euc++;
		}
		else
		{						/* should be ASCII */
			len--;
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> EUC_KR
 */
static void
mic2euc_kr(unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == LC_KS5601)
		{
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 > 0x7f)
		{						/* cannot convert to EUC_KR! */
			mic--;
			printBogusChar(&mic, &p);
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * EUC_CN ---> MIC
 */
static void
euc_cn2mic(unsigned char *euc, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *euc++))
	{
		if (c1 & 0x80)
		{
			len -= 2;
			*p++ = LC_GB2312_80;
			*p++ = c1;
			*p++ = *euc++;
		}
		else
		{						/* should be ASCII */
			len--;
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> EUC_CN
 */
static void
mic2euc_cn(unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == LC_GB2312_80)
		{
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 > 0x7f)
		{						/* cannot convert to EUC_CN! */
			mic--;
			printBogusChar(&mic, &p);
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * EUC_TW ---> MIC
 */
static void
euc_tw2mic(unsigned char *euc, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *euc++))
	{
		if (c1 == SS2)
		{
			len -= 4;
			c1 = *euc++;		/* plane No. */
			if (c1 == 0xa1)
				*p++ = LC_CNS11643_1;
			else if (c1 == 0xa2)
				*p++ = LC_CNS11643_2;
			else
			{
				*p++ = 0x9d;	/* LCPRV2 */
				*p++ = 0xa3 - c1 + LC_CNS11643_3;
			}
			*p++ = *euc++;
			*p++ = *euc++;
		}
		else if (c1 & 0x80)
		{						/* CNS11643-1 */
			len -= 2;
			*p++ = LC_CNS11643_1;
			*p++ = c1;
			*p++ = *euc++;
		}
		else
		{						/* should be ASCII */
			len--;
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> EUC_TW
 */
static void
mic2euc_tw(unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == LC_CNS11643_1 || c1 == LC_CNS11643_2)
		{
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 == 0x9d)
		{						/* LCPRV2? */
			*p++ = SS2;
			*p++ = c1 - LC_CNS11643_3 + 0xa3;
			*p++ = *mic++;
			*p++ = *mic++;
		}
		else if (c1 > 0x7f)
		{						/* cannot convert to EUC_TW! */
			mic--;
			printBogusChar(&mic, &p);
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

/*
 * Big5 ---> MIC
 */
static void
big52mic(unsigned char *big5, unsigned char *p, int len)
{
	unsigned short c1;
	unsigned short big5buf,
				cnsBuf;
	unsigned char lc;
	char		bogusBuf[2];
	int			i;

	while (len > 0 && (c1 = *big5++))
	{
		if (c1 <= 0x7fU)
		{						/* ASCII */
			len--;
			*p++ = c1;
		}
		else
		{
			len -= 2;
			big5buf = c1 << 8;
			c1 = *big5++;
			big5buf |= c1;
			cnsBuf = BIG5toCNS(big5buf, &lc);
			if (lc != 0)
			{
				if (lc == LC_CNS11643_3 || lc == LC_CNS11643_4)
				{
					*p++ = 0x9d;/* LCPRV2 */
				}
				*p++ = lc;		/* Plane No. */
				*p++ = (cnsBuf >> 8) & 0x00ff;
				*p++ = cnsBuf & 0x00ff;
			}
			else
			{					/* cannot convert */
				big5 -= 2;
				*p++ = '(';
				for (i = 0; i < 2; i++)
				{
					sprintf(bogusBuf, "%02x", *big5++);
					*p++ = bogusBuf[0];
					*p++ = bogusBuf[1];
				}
				*p++ = ')';
			}
		}
	}
	*p = '\0';
}

/*
 * MIC ---> Big5
 */
static void
mic2big5(unsigned char *mic, unsigned char *p, int len)
{
	int			l;
	unsigned short c1;
	unsigned short big5buf,
				cnsBuf;

	while (len > 0 && (c1 = *mic))
	{
		l = pg_mic_mblen(mic++);
		len -= l;

		/* 0x9d means LCPRV2 */
		if (c1 == LC_CNS11643_1 || c1 == LC_CNS11643_2 || c1 == 0x9d)
		{
			if (c1 == 0x9d)
			{
				c1 = *mic++;	/* get plane no. */
			}
			cnsBuf = (*mic++) << 8;
			cnsBuf |= (*mic++) & 0x00ff;
			big5buf = CNStoBIG5(cnsBuf, c1);
			if (big5buf == 0)
			{					/* cannot convert to Big5! */
				mic -= l;
				printBogusChar(&mic, &p);
			}
			else
			{
				*p++ = (big5buf >> 8) & 0x00ff;
				*p++ = big5buf & 0x00ff;
			}
		}
		else if (c1 <= 0x7f)	/* ASCII */
			*p++ = c1;
		else
		{						/* cannot convert to Big5! */
			mic--;
			printBogusChar(&mic, &p);
		}
	}
	*p = '\0';
}

/*
 * LATINn ---> MIC
 */
static void
latin2mic(unsigned char *l, unsigned char *p, int len, int lc)
{
	int			c1;

	while (len-- > 0 && (c1 = *l++))
	{
		if (c1 > 0x7f)
		{						/* Latin1? */
			*p++ = lc;
		}
		*p++ = c1;
	}
	*p = '\0';
}

/*
 * MIC ---> LATINn
 */
static void
mic2latin(unsigned char *mic, unsigned char *p, int len, int lc)
{
	int			c1;

	while (len > 0 && (c1 = *mic))
	{
		len -= pg_mic_mblen(mic++);

		if (c1 == lc)
			*p++ = *mic++;
		else if (c1 > 0x7f)
		{
			mic--;
			printBogusChar(&mic, &p);
		}
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
	}
	*p = '\0';
}

static void
latin12mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_ISO8859_1);
}
static void
mic2latin1(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_ISO8859_1);
}
static void
latin22mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_ISO8859_2);
}
static void
mic2latin2(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_ISO8859_2);
}
static void
latin32mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_ISO8859_3);
}
static void
mic2latin3(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_ISO8859_3);
}
static void
latin42mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_ISO8859_4);
}
static void
mic2latin4(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_ISO8859_4);
}

#ifdef NOT_USED
static void
latin52mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_ISO8859_5);
}
static void
mic2latin5(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_ISO8859_5);
}

#endif

/*
 * ASCII ---> MIC
 */
static void
ascii2mic(unsigned char *l, unsigned char *p, int len)
{
	int			c1;

	while (len-- > 0 && (c1 = *l++))
		*p++ = (c1 & 0x7f);
	*p = '\0';
}

/*
 * MIC ---> ASCII
 */
static void
mic2ascii(unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len-- > 0 && (c1 = *mic))
	{
		if (c1 > 0x7f)
			printBogusChar(&mic, &p);
		else
		{						/* should be ASCII */
			*p++ = c1;
		}
		mic++;
	}
	*p = '\0';
}

/*
 * Cyrillic support
 * currently supported Cyrillic encodings:
 *
 * KOI8-R (this is the charset for the mule internal code
 *		for Cyrillic)
 * ISO-8859-5
 * Microsoft's CP1251(windows-1251)
 * Alternativny Variant (MS-DOS CP866)
 */

/* koi2mic: KOI8-R to Mule internal code */
static void
koi2mic(unsigned char *l, unsigned char *p, int len)
{
	latin2mic(l, p, len, LC_KOI8_R);
}

/* mic2koi: Mule internal code to KOI8-R */
static void
mic2koi(unsigned char *mic, unsigned char *p, int len)
{
	mic2latin(mic, p, len, LC_KOI8_R);
}

/*
 * latin2mic_with_table: a generic single byte charset encoding
 * conversion from a local charset to the mule internal code.
 * with a encoding conversion table.
 * the table is ordered according to the local charset,
 * starting from 128 (0x80). each entry in the table
 * holds the corresponding code point for the mule internal code.
 */
static void
latin2mic_with_table(
					 unsigned char *l,	/* local charset string (source) */
					 unsigned char *p,	/* pointer to store mule internal
										 * code (destination) */
					 int len,	/* length of l */
					 int lc,	/* leading character of p */
					 unsigned char *tab /* code conversion table */
)
{
	unsigned char c1,
				c2;

	while (len-- > 0 && (c1 = *l++))
	{
		if (c1 < 128)
			*p++ = c1;
		else
		{
			c2 = tab[c1 - 128];
			if (c2)
			{
				*p++ = lc;
				*p++ = c2;
			}
			else
			{
				*p++ = ' ';		/* cannot convert */
			}
		}
	}
	*p = '\0';
}

/*
 * mic2latin_with_table: a generic single byte charset encoding
 * conversion from the mule internal code to a local charset
 * with a encoding conversion table.
 * the table is ordered according to the second byte of the mule
 * internal code starting from 128 (0x80).
 * each entry in the table
 * holds the corresponding code point for the local code.
 */
static void
mic2latin_with_table(
					 unsigned char *mic,		/* mule internal code
												 * (source) */
					 unsigned char *p,	/* local code (destination) */
					 int len,	/* length of p */
					 int lc,	/* leading character */
					 unsigned char *tab /* code conversion table */
)
{

	unsigned char c1,
				c2;

	while (len-- > 0 && (c1 = *mic++))
	{
		if (c1 < 128)
			*p++ = c1;
		else if (c1 == lc)
		{
			c1 = *mic++;
			len--;
			c2 = tab[c1 - 128];
			if (c2)
				*p++ = c2;
			else
			{
				*p++ = ' ';		/* cannot convert */
			}
		}
		else
		{
			*p++ = ' ';			/* bogus character */
		}
	}
	*p = '\0';
}

/* iso2mic: ISO-8859-5 to Mule internal code */
static void
iso2mic(unsigned char *l, unsigned char *p, int len)
{
	static unsigned char iso2koi[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa,
		0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
		0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe,
		0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
		0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda,
		0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
		0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde,
		0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	latin2mic_with_table(l, p, len, LC_KOI8_R, iso2koi);
}

/* mic2iso: Mule internal code to ISO8859-5 */
static void
mic2iso(unsigned char *mic, unsigned char *p, int len)
{
	static unsigned char koi2iso[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xee, 0xd0, 0xd1, 0xe6, 0xd4, 0xd5, 0xe4, 0xd3,
		0xe5, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
		0xdf, 0xef, 0xe0, 0xe1, 0xe2, 0xe3, 0xd6, 0xd2,
		0xec, 0xeb, 0xd7, 0xe8, 0xed, 0xe9, 0xe7, 0xea,
		0xce, 0xb0, 0xb1, 0xc6, 0xb4, 0xb5, 0xc4, 0xb3,
		0xc5, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
		0xbf, 0xcf, 0xc0, 0xc1, 0xc2, 0xc3, 0xb6, 0xb2,
		0xcc, 0xcb, 0xb7, 0xc8, 0xcd, 0xc9, 0xc7, 0xca
	};

	mic2latin_with_table(mic, p, len, LC_KOI8_R, koi2iso);
}

/* win2mic: CP1251 to Mule internal code */
static void
win2mic(unsigned char *l, unsigned char *p, int len)
{
	static unsigned char win2koi[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xbd, 0x00, 0x00,
		0xb3, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x00, 0xb7,
		0x00, 0x00, 0xb6, 0xa6, 0xad, 0x00, 0x00, 0x00,
		0xa3, 0x00, 0xa4, 0x00, 0x00, 0x00, 0x00, 0xa7,
		0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa,
		0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
		0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe,
		0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
		0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda,
		0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
		0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde,
		0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1
	};

	latin2mic_with_table(l, p, len, LC_KOI8_R, win2koi);
}

/* mic2win: Mule internal code to CP1251 */
static void
mic2win(unsigned char *mic, unsigned char *p, int len)
{
	static unsigned char koi2win[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xb8, 0xba, 0x00, 0xb3, 0xbf,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xa8, 0xaa, 0x00, 0xb2, 0xaf,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xa5, 0x00, 0x00,
		0xfe, 0xe0, 0xe1, 0xf6, 0xe4, 0xe5, 0xf4, 0xe3,
		0xf5, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
		0xef, 0xff, 0xf0, 0xf1, 0xf2, 0xf3, 0xe6, 0xe2,
		0xfc, 0xfb, 0xe7, 0xf8, 0xfd, 0xf9, 0xf7, 0xfa,
		0xde, 0xc0, 0xc1, 0xd6, 0xc4, 0xc5, 0xd4, 0xc3,
		0xd5, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
		0xcf, 0xdf, 0xd0, 0xd1, 0xd2, 0xd3, 0xc6, 0xc2,
		0xdc, 0xdb, 0xc7, 0xd8, 0xdd, 0xd9, 0xd7, 0xda
	};

	mic2latin_with_table(mic, p, len, LC_KOI8_R, koi2win);
}

/* alt2mic: CP866 to Mule internal code */
static void
alt2mic(unsigned char *l, unsigned char *p, int len)
{
	static unsigned char alt2koi[] = {
		0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa,
		0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
		0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe,
		0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
		0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda,
		0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xbd, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde,
		0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
		0xb3, 0xa3, 0xb4, 0xa4, 0xb7, 0xa7, 0x00, 0x00,
		0xb6, 0xa6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	latin2mic_with_table(l, p, len, LC_KOI8_R, alt2koi);
}

/* mic2alt: Mule internal code to CP866 */
static void
mic2alt(unsigned char *mic, unsigned char *p, int len)
{
	static unsigned char koi2alt[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xf1, 0xf3, 0x00, 0xf9, 0xf5,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xad, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xf0, 0xf2, 0x00, 0xf8, 0xf4,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xbd, 0x00, 0x00,
		0xee, 0xa0, 0xa1, 0xe6, 0xa4, 0xa5, 0xe4, 0xa3,
		0xe5, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
		0xaf, 0xef, 0xe0, 0xe1, 0xe2, 0xe3, 0xa6, 0xa2,
		0xec, 0xeb, 0xa7, 0xe8, 0xed, 0xe9, 0xe7, 0xea,
		0x9e, 0x80, 0x81, 0x96, 0x84, 0x85, 0x94, 0x83,
		0x95, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
		0x8f, 0x9f, 0x90, 0x91, 0x92, 0x93, 0x86, 0x82,
		0x9c, 0x9b, 0x87, 0x98, 0x9d, 0x99, 0x97, 0x9a
	};

	mic2latin_with_table(mic, p, len, LC_KOI8_R, koi2alt);
}

/*
 * end of Cyrillic support
 */


/*-----------------------------------------------------------------
 * WIN1250
 * Microsoft's CP1250(windows-1250)
 *-----------------------------------------------------------------*/
static void
win12502mic(unsigned char *l, unsigned char *p, int len)
{
	static unsigned char win1250_2_iso88592[] = {
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0xA9, 0x8B, 0xA6, 0xAB, 0xAE, 0xAC,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0x98, 0x99, 0xB9, 0x9B, 0xB6, 0xBB, 0xBE, 0xBC,
		0xA0, 0xB7, 0xA2, 0xA3, 0xA4, 0xA1, 0x00, 0xA7,
		0xA8, 0x00, 0xAA, 0x00, 0x00, 0xAD, 0x00, 0xAF,
		0xB0, 0x00, 0xB2, 0xB3, 0xB4, 0x00, 0x00, 0x00,
		0xB8, 0xB1, 0xBA, 0x00, 0xA5, 0xBD, 0xB5, 0xBF,
		0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
		0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
		0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
		0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
		0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
		0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
		0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
		0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
	};

	latin2mic_with_table(l, p, len, LC_ISO8859_2, win1250_2_iso88592);
}
static void
mic2win1250(unsigned char *mic, unsigned char *p, int len)
{
	static unsigned char iso88592_2_win1250[] = {
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0x98, 0x99, 0x00, 0x9B, 0x00, 0x00, 0x00, 0x00,
		0xA0, 0xA5, 0xA2, 0xA3, 0xA4, 0xBC, 0x8C, 0xA7,
		0xA8, 0x8A, 0xAA, 0x8D, 0x8F, 0xAD, 0x8E, 0xAF,
		0xB0, 0xB9, 0xB2, 0xB3, 0xB4, 0xBE, 0x9C, 0xA1,
		0xB8, 0x9A, 0xBA, 0x9D, 0x9F, 0xBD, 0x9E, 0xBF,
		0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
		0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
		0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
		0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
		0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
		0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
		0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
		0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
	};

	mic2latin_with_table(mic, p, len, LC_ISO8859_2, iso88592_2_win1250);
}

/*-----------------------------------------------------------------*/

pg_encoding_conv_tbl pg_conv_tbl[] = {
	{SQL_ASCII, "SQL_ASCII", 0, ascii2mic, mic2ascii},	/* SQL/ACII */
	{EUC_JP, "EUC_JP", 0, euc_jp2mic, mic2euc_jp},		/* EUC_JP */
	{EUC_CN, "EUC_CN", 0, euc_cn2mic, mic2euc_cn},		/* EUC_CN */
	{EUC_KR, "EUC_KR", 0, euc_kr2mic, mic2euc_kr},		/* EUC_KR */
	{EUC_TW, "EUC_TW", 0, euc_tw2mic, mic2euc_tw},		/* EUC_TW */
	{UNICODE, "UNICODE", 0, 0, 0},		/* UNICODE */
	{MULE_INTERNAL, "MULE_INTERNAL", 0, 0, 0},	/* MULE_INTERNAL */
	{LATIN1, "LATIN1", 0, latin12mic, mic2latin1},		/* ISO 8859 Latin 1 */
	{LATIN2, "LATIN2", 0, latin22mic, mic2latin2},		/* ISO 8859 Latin 2 */
	{LATIN3, "LATIN3", 0, latin32mic, mic2latin3},		/* ISO 8859 Latin 3 */
	{LATIN4, "LATIN4", 0, latin42mic, mic2latin4},		/* ISO 8859 Latin 4 */
	{LATIN5, "LATIN5", 0, iso2mic, mic2iso},	/* ISO 8859 Latin 5 */
	{KOI8, "KOI8", 0, koi2mic, mic2koi},		/* KOI8-R */
	{WIN, "WIN", 0, win2mic, mic2win},	/* CP1251 */
	{ALT, "ALT", 0, alt2mic, mic2alt},	/* CP866 */
	{SJIS, "SJIS", 1, sjis2mic, mic2sjis},		/* SJIS */
	{BIG5, "BIG5", 1, big52mic, mic2big5},		/* Big5 */
	{WIN1250, "WIN1250", 1, win12502mic, mic2win1250},	/* WIN 1250 */
	{-1, "", 0, 0, 0}			/* end mark */
};

#ifdef DEBUGMAIN
#include "utils/mcxt.h"
/*
 *	testing for sjis2mic() and mic2sjis()
 */

int
main()
{
	unsigned char eucbuf[1024];
	unsigned char sjisbuf[1024];
	unsigned char sjis[] = {0x81, 0x40, 0xa1, 0xf0, 0x40, 0xf0, 0x9e, 0xf5, 0x40, 0xfa, 0x40, 0xfa, 0x54, 0xfa, 0x7b, 0x00};

	int			i;

	sjis2mic(sjis, eucbuf, 1024);
	for (i = 0; i < 1024; i++)
	{
		if (eucbuf[i])
			printf("%02x ", eucbuf[i]);
		else
		{
			printf("\n");
			break;
		}
	}

	mic2sjis(eucbuf, sjisbuf, 1024);
	for (i = 0; i < 1024; i++)
	{
		if (sjisbuf[i])
			printf("%02x ", sjisbuf[i]);
		else
		{
			printf("\n");
			break;
		}
	}

	return (0);
}

void
elog(int lev, const char *fmt,...)
{
};
MemoryContext CurrentMemoryContext;
Pointer
MemoryContextAlloc(MemoryContext context, Size size)
{
};
Pointer
MemoryContextRealloc(MemoryContext context,
					 Pointer pointer,
					 Size size)
{
};
void
MemoryContextFree(MemoryContext context, Pointer pointer)
{
};

#endif
