
'use strict';

function forceChosenVideoCodec(sdp, codec) {
	return maybePreferCodec(sdp, 'video', 'send', codec);
}

function forceChosenAudioCodec(sdp, codec) {
	return maybePreferCodec(sdp, 'audio', 'send', codec);
}

// 
// Copied from the AppRTC's sdputils.js and some were modified.
//

// Add an a=fmtp: x-google-min-bitrate=kbps line, if videoSendInitialBitrate
// is specified. We'll also add a x-google-min-bitrate value, since the max
// must be >= the min.
function setVideoInitialBitRate(sdp, codec, minBitrate, maxBitrate) {

    minBitrate = Number(minBitrate);
    maxBitrate = Number(maxBitrate);
    if (minBitrate > maxBitrate) {
        trace('Clamping initial bitrate to max bitrate of ' +
                maxBitrate + ' kbps.');
        minBitrate = maxBitrate;
    }

    var sdpLines = sdp.split('\r\n');

    // Search for m line.
    var mLineIndex = findLine(sdpLines, 'm=', 'video');
    if (mLineIndex === null) {
        trace('Failed to find video m-line');
        return sdp;
    }

    sdp = setCodecParam(sdp, codec, 'x-google-min-bitrate',
            minBitrate.toString());
    sdp = setCodecParam(sdp, codec, 'x-google-max-bitrate',
            maxBitrate.toString());
    sdp = setCodecParam(sdp, codec, 'level-asymmetry-allowed', '1');
    sdp = setCodecParam(sdp, codec, 'packetization-mode', '1');
    sdp = setCodecParam(sdp, codec, 'profile-level-id', '42e01f');

    return sdp;
}

// Set fmtp param to specific codec in SDP. If param does not exists, add it.
function setCodecParam(sdp, codec, param, value) {
  var sdpLines = sdp.split('\r\n');

  var fmtpLineIndex = findFmtpLine(sdpLines, codec);

  var fmtpObj = {};
  if (fmtpLineIndex === null) {
    var index = findLine(sdpLines, 'a=rtpmap', codec);
    if (index === null) {
      return sdp;
    }
    trace("setCodecParam findLine index: " + index );
    trace("setCodecParam findLine : " + sdpLines[index] );
    var payload = getCodecPayloadTypeFromLine(sdpLines[index]);
    fmtpObj.pt = payload.toString();
    fmtpObj.params = {};
    fmtpObj.params[param] = value;
    sdpLines.splice(index + 1, 0, writeFmtpLine(fmtpObj));
  } else {
    fmtpObj = parseFmtpLine(sdpLines[fmtpLineIndex]);
    fmtpObj.params[param] = value;
    sdpLines[fmtpLineIndex] = writeFmtpLine(fmtpObj);
  }

  sdp = sdpLines.join('\r\n');
  return sdp;
}

// Generate an fmtp line from an object including 'pt' and 'params'.
function writeFmtpLine(fmtpObj) {
  if (!fmtpObj.hasOwnProperty('pt') || !fmtpObj.hasOwnProperty('params')) {
    return null;
  }
  var pt = fmtpObj.pt;
  var params = fmtpObj.params;
  var keyValues = [];
  var i = 0;
  for (var key in params) {
    keyValues[i] = key + '=' + params[key];
    ++i;
  }
  if (i === 0) {
    return null;
  }
  return 'a=fmtp:' + pt.toString() + ' ' + keyValues.join('; ');
}

// Find fmtp attribute for |codec| in |sdpLines|.
function findFmtpLine(sdpLines, codec) {
  // Find payload of codec.
  var payload = getCodecPayloadType(sdpLines, codec);
  // Find the payload in fmtp line.
  return payload ? findLine(sdpLines, 'a=fmtp:' + payload.toString()) : null;
}

// Split an fmtp line into an object including 'pt' and 'params'.
function parseFmtpLine(fmtpLine) {
  var fmtpObj = {};
  var spacePos = fmtpLine.indexOf(' ');
  var keyValues = fmtpLine.substring(spacePos + 1).split('; ');

  var pattern = new RegExp('a=fmtp:(\\d+)');
  var result = fmtpLine.match(pattern);
  if (result && result.length === 2) {
    fmtpObj.pt = result[1];
  } else {
    return null;
  }

  var params = {};
  for (var i = 0; i < keyValues.length; ++i) {
    var pair = keyValues[i].split('=');
    if (pair.length === 2) {
      params[pair[0]] = pair[1];
    }
  }
  fmtpObj.params = params;

  return fmtpObj;
}

// Gets the codec payload type from an a=rtpmap:X line.
function getCodecPayloadTypeFromLine(sdpLine) {
  var pattern = new RegExp('a=rtpmap:(\\d+) [a-zA-Z0-9-]+\\/\\d+');
  var result = sdpLine.match(pattern);
  return (result && result.length === 2) ? result[1] : null;
}

// Sets |codec| as the default |type| codec if it's present.
// The format of |codec| is 'NAME/RATE', e.g. 'opus/48000'.
function maybePreferCodec(sdp, type, dir, codec) {
	var str = type + ' ' + dir + ' codec';
	if (codec === '') {
		trace('No preference on ' + str + '.');
		return sdp;
	}

	trace('Prefer ' + str + ': ' + codec);	// kclyu

	var sdpLines = sdp.split('\r\n');

	// Search for m line.
	var mLineIndex = findLine(sdpLines, 'm=', type);
	if (mLineIndex === null) {
		trace('* not found error: ' + str + ': ' + codec );	// kclyu
		return sdp;
	}

	// If the codec is available, set it as the default in m line.
	var codecIndex = findLine(sdpLines, 'a=rtpmap', codec);
	trace('mLineIndex Line: ' +  sdpLines[mLineIndex] );
	trace('found Prefered Codec in : ' + codecIndex + ': ' + sdpLines[codecIndex] );
	trace('codecIndex', codecIndex);
	if (codecIndex) {
		var payload = getCodecPayloadTypeFromLine(sdpLines[codecIndex]);
		if (payload) {
			sdpLines[mLineIndex] = setDefaultCodec(sdpLines[mLineIndex], payload);
			//sdpLines[mLineIndex] = setDefaultCodecAndRemoveOthers(sdpLines, sdpLines[mLineIndex], payload);
		}
	}

	trace('Modified LineIndex Line: ' +  sdpLines[mLineIndex] );
	sdp = sdpLines.join('\r\n');
	return sdp;
}

// Find the line in sdpLines that starts with |prefix|, and, if specified,
// contains |substr| (case-insensitive search).
function findLine(sdpLines, prefix, substr) {
	return findLineInRange(sdpLines, 0, -1, prefix, substr);
}

// Find the line in sdpLines[startLine...endLine - 1] that starts with |prefix|
// and, if specified, contains |substr| (case-insensitive search).
function findLineInRange(sdpLines, startLine, endLine, prefix, substr) {
	var realEndLine = endLine !== -1 ? endLine : sdpLines.length;
	for (var i = startLine; i < realEndLine; ++i) {
		if (sdpLines[i].indexOf(prefix) === 0) {
			if (!substr ||
					sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {

				return i;
			}
		}
	}
	return null;
}


// Gets the codec payload type from sdp lines.
function getCodecPayloadType(sdpLines, codec) {
  var index = findLine(sdpLines, 'a=rtpmap', codec);
  return index ? getCodecPayloadTypeFromLine(sdpLines[index]) : null;
}

// Gets the codec payload type from an a=rtpmap:X line.
function getCodecPayloadTypeFromLine(sdpLine) {
  var pattern = new RegExp('a=rtpmap:(\\d+) [a-zA-Z0-9-]+\\/\\d+');
  var result = sdpLine.match(pattern);
  return (result && result.length === 2) ? result[1] : null;
}

// Returns a new m= line with the specified codec as the first one.
function setDefaultCodec(mLine, payload) {
	var elements = mLine.split(' ');

	// Just copy the first three parameters; codec order starts on fourth.
	var newLine = elements.slice(0, 3);

	// Put target payload first and copy in the rest.
	newLine.push(payload);
	for (var i = 3; i < elements.length; i++) {
		if (elements[i] !== payload) {
			newLine.push(elements[i]);
		}
	}
	return newLine.join(' ');
}


function setDefaultCodecAndRemoveOthers(sdpLines, mLine, payload) {
	var elements = mLine.split(' ');

	// Just copy the first three parameters; codec order starts on fourth.
	var newLine = elements.slice(0, 3);
	

	// Put target payload first and copy in the rest.
	newLine.push(payload);
	for (var i = 3; i < elements.length; i++) {
		if (elements[i] !== payload) {

			//  continue to remove all matching lines
			for(var line_removed = true;line_removed;) {
				line_removed = RemoveLineInRange(sdpLines, 0, -1, "a=rtpmap", elements[i] );
			}
			//  continue to remove all matching lines
			for(var line_removed = true;line_removed;) {
				line_removed = RemoveLineInRange(sdpLines, 0, -1, "a=rtcp-fb", elements[i] );
			}
		}
	}
	return newLine.join(' ');
}

function RemoveLineInRange(sdpLines, startLine, endLine, prefix, substr) {
	var realEndLine = endLine !== -1 ? endLine : sdpLines.length;
	for (var i = startLine; i < realEndLine; ++i) {
		if (sdpLines[i].indexOf(prefix) === 0) {
			if (!substr ||
					sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {
				var str = "Deleting(index: " + i + ") : " + sdpLines[i];
				trace(str);
				sdpLines.splice(i, 1);
				return true;
			}
		}
	}
	return false;
}

