/*
    Copyright 2013 Alex Danilo

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
var OUTBYTES = 8;
var INBYTES	= 7;
var PADCHAR = '!';
var BAD = 0xFF;
var PAD = 0xFE;

function b128strToBlob(str) {
    var bytes = (str.length / OUTBYTES) * INBYTES;
    var image = new Uint8Array(bytes);
    decodeBase128(image, str);

    return makeBlobUrl(image);    
}

var dtab;

function decodeBase128(out, string) {
	var 	ind = new Uint8Array(OUTBYTES);
	var 	topbits, mask;
	var 	i, str, count, len;

    str = count = 0;
	while (str < string.length) {
		len = 0;
		for (i = 0; i < OUTBYTES; i++) {
			ind[i] = string.charCodeAt(str + i);
			if ((str + i) > string.length)
				break;
			len++;
		}
		if (len > 0) {
			topbits = dtab[ind[len - 1]];
			mask = 0x40;
			for (len = i = 0; i < INBYTES; i++) {
				if (ind[i] == PADCHAR)
					break;
				len++;
				ind[i] = dtab[ind[i]];
				if (ind[i] == BAD)		/* Illegal encoding character, bail out */
					return;
				if (topbits & mask)
					ind[i] |= 0x80;
				mask >>= 1;
			}
			for (i = 0; i < len; i++) {
				out[count + i] = ind[i];
			}
		}
		str += OUTBYTES;
		count += INBYTES;
	}
}

function initDtab() {
	var i;

	dtab = new Uint8Array(256);
	for (i = 0; i < 160; i++)
		dtab[i] = BAD;
	dtab[33] = PAD;
	for (i = 35; i < 92; i++)
		dtab[i] = i - 35;
	i++;					// Skip '\\'
	for (; i < 127; i++)
		dtab[i] = i - 36;
	for (i = 160; i < 256; i++)
		dtab[i] = i - 69; 	// (160 - 127) + 35
}

function makeBlobUrl(image) {
	var blob;

    if (typeof (URL) !== 'undefined') {
	   blob = new Blob([image], {type: "image/jpeg"});
	   return URL.createObjectURL(blob);
	}
    else if (webkitURL) {
	   blob = new Blob([image.buffer], {type: "image/jpeg"});
       return webkitURL.createObjectURL(blob);
    }
    else {
        return null;
    }
}

initDtab();