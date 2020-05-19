<?
require 'fppdefines.php';
?>
<script type="text/javascript" src="jquery/jcanvas.js"></script>
<script>

var config = {};  // Plugin configuration
var devices = {}; // Cached list of V4L2 Devices

function OutputAdjusted(adj) {
    var parts = config['resolution'].split(':');
    var w  = parseInt(parts[0]);
    var h  = parseInt(parts[1]);
    var ow = parseInt($('#outputWidth').val());
    var oh = parseInt($('#outputHeight').val());
    var cl = parseInt($('#cropL').val());
    var ct = parseInt($('#cropT').val());

    // Sanity check our crop
    switch (adj) {
        case 'W':   if ((ow + cl) > w) {
                        if (cl > 0) {
                            cl--;
                            $('#cropL').val(cl);
                        } else {
                            ow--;
                            $('#outputWidth').val(ow);
                        }
                    }
                    break;
        case 'L':   if ((ow + cl) > w) {
                        if (cl > 0) {
                            cl--;
                            $('#cropL').val(cl);
                        } else {
                            ow = w;
                            $('#outputWidth').val(ow);
                        }
                    }
                    break;
        case 'H':   if ((oh + ct) > h) {
                        if (ct > 0) {
                            ct--;
                            $('#cropT').val(ct);
                        } else {
                            oh--;
                            $('#outputHeight').val(oh);
                        }
                    }
                    break;
        case 'T':   if ((oh + ct) > h) {
                        if (ct > 0) {
                            ct--;
                            $('#cropT').val(ct);
                        } else {
                            oh = h;
                            $('#outputHeight').val(oh);
                        }
                    }
                    break;
    }

    DrawCapturedFrame();

    SavePluginConfig();
}

function SavePluginConfig() {
    config['deviceName'] = $('#deviceName').val();
    config['resolution'] = $('#resolution').val();
    config['startChannel'] = parseInt($('#startChannel').val());
    config['captureOn'] = parseInt($('#captureOn').val());
    config['forceChannelOutput'] = parseInt($('#forceChannelOutput').val());
    config['outputWidth'] = parseInt($('#outputWidth').val());
    config['outputHeight'] = parseInt($('#outputHeight').val());
    config['cropL'] = parseInt($('#cropL').val());
    config['cropT'] = parseInt($('#cropT').val());

    var parts = config['resolution'].split(':');
    config['width'] = parseInt(parts[0]);
    config['height'] = parseInt(parts[1]);
    config['fps'] = parseInt(parts[2]);

    var configStr = JSON.stringify(config, null, 2);
    $.post('/api/configfile/plugin.fpp-VideoCapture.json', configStr).done(function(data) {
        $.jGrowl('Video Capture Config Saved');
        SetRestartFlag(2);
        CheckRestartRebootFlags();
    }).fail(function() {
        alert('Error, could not save plugin.fpp-VideoCapture.json config file.');
    });
}

function LoadConfig() {
    $.ajax({
        url: 'api/configfile/plugin.fpp-VideoCapture.json',
        type: 'GET',
        async: false,
        dataType: 'json',
        success: function(data) {
            config = data;
        }
    });

    if (!config.hasOwnProperty('deviceName'))
        config['deviceName'] = '/dev/video0';

    if (!config.hasOwnProperty('width'))
        config['width'] = 160;
            
    if (!config.hasOwnProperty('height'))
        config['height'] = 120;
            
    if (!config.hasOwnProperty('fps'))
        config['fps'] = 5;

    if (!config.hasOwnProperty('startChannel'))
        config['startChannel'] = 1;

    if (!config.hasOwnProperty('captureOn'))
        config['captureOn'] = 1;

    if (!config.hasOwnProperty('forceChannelOutput'))
        config['forceChannelOutput'] = 0;

    if (!config.hasOwnProperty('outputMode'))
        config['outputMode'] = 'crop';

    if (!config.hasOwnProperty('outputWidth'))
        config['outputWidth'] = config['width'];
            
    if (!config.hasOwnProperty('outputHeight'))
        config['outputHeight'] = config['height'];
            
    if (!config.hasOwnProperty('cropL'))
        config['cropL'] = 0;
            
    if (!config.hasOwnProperty('cropT'))
        config['cropT'] = 0;
            
    config['resolution'] = "" + config['width'] + ':' + config['height'] + ':' + config['fps'];

    $('#startChannel').val(config['startChannel']);
    $('#captureOn').val(config['captureOn']);
    $('#forceChannelOutput').val(config['forceChannelOutput']);
    $('#outputMode').val(config['outputMode']);
    $('#outputWidth').val(config['outputWidth']);
    $('#outputHeight').val(config['outputHeight']);
    $('#cropL').val(config['cropL']);
    $('#cropT').val(config['cropT']);
}

function LoadResolutionsForDevice() {
    var deviceName = $('#deviceName').val();

    var resolutions = "";
    keys = Object.keys(devices.devices[deviceName].resolutions);
    for (var i = 0; i < keys.length; i++) {
        resolutions += "<option value='" + keys[i] + "'";

        if (keys[i] == config['resolution']) {
            resolutions += ' selected';
        }

        resolutions += ">" + devices.devices[deviceName].resolutions[keys[i]] + "</option>";
    }
    $('#resolution').html(resolutions);
}

function LoadDevices() {
    $.get('plugin.php?plugin=fpp-VideoCapture&page=devices.php&nopage=1', function(data) {
        devices = data;

        var options = "";
        var keys = Object.keys(data.devices);
        for (var i = 0; i < keys.length; i++) {
            options += "<option value='" + keys[i] + "'";

            if (config['deviceName'] == keys[i])
                options += " selected";

            options += ">" + keys[i] + "</option>";
        }
        $('#deviceName').html(options);

        LoadResolutionsForDevice();
    });
}

var frameCaptured = false;
var pixelData = [];
function CaptureVideoFrame() {
    $.get('api/plugin-apis/VideoCapture/getFrame', function(data) {
        var c = 0;
        var lines = data.split('\n');
        for (var i = 3; i < lines.length; i++) {
            var colors = lines[i].split(' ');
            pixelData[c++] = parseInt(colors[0]);
            pixelData[c++] = parseInt(colors[1]);
            pixelData[c++] = parseInt(colors[2]);
        }

        frameCaptured = true;
        DrawCapturedFrame();
    });
}

var ctx;
var buffer;
var bctx;

function rgbToHex(r, g, b) {
  return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

function InitCanvas() {
    $('#vcCanvas').attr('width', config['width']);
    $('#vcCanvas').attr('height', config['height']);

    $('#vcCanvas').removeLayers();
    $('#vcCanvas').clearCanvas();

    $('#vCanvas').drawRect({
        layer: true,
        fillStyle: '#000',
        x: 0,
        y: 0,
        width: config['width'],
        height: config['height']
    });

    var c = document.getElementById('vcCanvas');
    ctx = c.getContext('2d');

    buffer = document.createElement('canvas');
    buffer.width = c.width;
    buffer.height = c.height;
    bctx = buffer.getContext('2d');

    bctx.fillStyle = '#000000';
    bctx.fillRect( 0, 0, c.width, c.height);
    ctx.drawImage(buffer, 0, 0);
}

function OverlayCropArea() {
    var parts = $('#resolution').val().split(':');
    var w  = parseInt(parts[0]);
    var h  = parseInt(parts[1]);
    var pixelsWide = parseInt($('#outputWidth').val());
    var pixelsHigh = parseInt($('#outputHeight').val());
    var l = parseInt($('#cropL').val());
    var r = l + pixelsWide - 1;
    var t = parseInt($('#cropT').val());
    var b = t + pixelsHigh - 1;

    bctx.fillStyle = '#00FF00';

    bctx.fillRect(l, t, pixelsWide, 1); // top
    bctx.fillRect(l, b, pixelsWide, 1); // bottom
    bctx.fillRect(l, t, 1, pixelsHigh); // left
    bctx.fillRect(r, t, 1, pixelsHigh); // right
}

function DrawCapturedFrame() {
    if (!frameCaptured) {
        return;
    }

    var width = config['width'];
    var height = config['height'];
    var b = 0;

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            bctx.fillStyle = rgbToHex(pixelData[b], pixelData[b+1], pixelData[b+2]);
            b += 3;

            bctx.fillRect(x, y, 1, 1);
        }
    }

    OverlayCropArea();

    ctx.drawImage(buffer, 0, 0);
}

$(document).ready(function() {
    $.jCanvas.defaults.fromCenter = false;

    LoadConfig();
    LoadDevices();
    InitCanvas();
    $(document).tooltip();
});

</script>

<div id="global" class="settings">
    <fieldset>
        <legend>Video Capture</legend>
        <b>Input Settings:</b><br>
        <table class='settingsTableWrapper'>
            <tr><td>Video Device:</td>
                <td><select id='deviceName' onChange='SavePluginConfig();'></select></td></tr>
            <tr><td>Webcam Resolution / FPS:</td>
                <td><select id='resolution' onChange='SavePluginConfig();'></select></td></tr>
            <tr><td>Default Capture State:</td>
                <td><select id='captureOn' onChange='SavePluginConfig();'>
                        <option value='1'>On</option>
                        <option value='0'>Off</option>
                    </select></td></tr>
        </table>
        <br>
        <b>Output Settings:</b><br>
        <table class='settingsTableWrapper'>
            <tr><td>Start Channel:</td>
                <td><input type='number' id='startChannel' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' onChange='SavePluginConfig();'></select></td></tr>
            <tr><td>Channel Output Thread:</td>
                <td><select id='forceChannelOutput' onChange='SavePluginConfig();'>
                        <option value='1'>Force On</option>
                        <option value='0'>Default</option>
                    </select></td></tr>
            <tr><td>Output Size (WxH):</td>
                <td><input type='number' id='outputWidth' min='0' max='160' onChange='OutputAdjusted("W");'> x
                    <input type='number' id='outputHeight' min='0' max='160' onChange='OutputAdjusted("H");'></td></tr>
            <tr><td>Output Sizing Mode:</td>
                <td><select id='sizingMode' onChange='SavePluginConfig();'>
                        <option value='crop'>Crop</option>
                        <!--
                        <option value='scale'>Scale</option>
                        -->
                    </select></td></tr>
            <tr class='cropInput'><td>Crop Left Offset:</td>
                <td><input type='number' id='cropL' min='0' max='160' onChange='OutputAdjusted("L");'></td></tr>
            <tr class='cropInput'><td>Crop Top Offset:</td>
                <td><input type='number' id='cropT' min='0' max='120' onChange='OutputAdjusted("T");'></td></tr>
        </table>

        <br>
        <b>Image Preview:</b><br>
        <canvas id='vcCanvas' width='160' height='120'></canvas><br>
        <input type='button' onClick='CaptureVideoFrame();' value='Capture Frame'><br>
        NOTE: Cropped area shown in green box.<br>
    </fieldset>
</div>
