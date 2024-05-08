var count_image = 20;
var web_socket = null;
var store_item = false;
var get_item = false;
var face_detect = false;
var status_ = false;
const displayMessageTimeout = 5000;
const receiveMessageTimeout = 10000;
var globalMessageElement;
var messageToSend = "start_finger_";
var message_old_image = "blink_led_";
var message_continue_backend = "continue";
var downloaded_image = "downloaded_image";
var comparefinger = "compare_finger";
var topicToSend = "/topic/hungmai_wb_publish";
var topicToSend_py = "/topic/hungmai_py_subscribe";
var number = 0;
var number_old_image = 0;
var number_empty = 0;
var faceDetectionCount = 0;
var faceDetectedLastTime = false;
var text_input = "capture";
var count_print_message = 0;
var status_image = false;
var status_back = false;
var status_back_finger = false;
var end_image = false;
var old_image = false;
var emptyCountElement = document.getElementById("empty-count");
// Tạo một MQTT client instance
var client = new Paho.MQTT.Client("mqtt.eclipseprojects.io", Number(80), "/mqtt", "myclientid_" + parseInt(Math.random() * 100, 10));
// Khi connect thành công, subscribe đến topic "/topic/hungmai_py_publish"
client.onConnectionLost = onConnectionLost;
client.onMessageArrived = onMessageArrived;

client.connect({
    onSuccess: function () {
        console.log("Connected to MQTT broker");
        client.subscribe("/topic/hungmai_py_publish");
    },
    onFailure: function (message) {
        console.log("Connection failed: " + message.errorMessage);
    },
    timeout: 1000, // Thời gian chờ (giây) trước khi bỏ kết nối
    keepAliveInterval: 12000 // Thời gian giữ kết nối (giây)
});

// Hàm callback được gọi khi kết nối bị mất
function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("Connection lost: " + responseObject.errorMessage);
        setTimeout(function() {
            client.connect({
                onSuccess: function () {
                    console.log("Reconnected to MQTT broker");
                    client.subscribe("/topic/hungmai_py_publish");
                },
                onFailure: function (message) {
                    console.log("Reconnection failed: " + message.errorMessage);
                }
            });
        }, 2); // Thử kết nối lại sau 2 mgiây
    }
}

// Hàm callback được gọi khi nhận được một message mới
function onMessageArrived(message) {
    console.log("Received message: " + message.payloadString);
    // Bắt đầu chạy ứng dụng
    if (message.payloadString.includes("start_count_")) {
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        document.getElementById('download').style.display = 'none'; // Ẩn nút "Tiếp tục"
        var matches = message.payloadString.match(/\d+/);
        console.log("matches" + matches);
        if (matches) {
        number = parseInt(matches[0]); 
        }
        number_empty = 7 - number;
        // Cập nhật nội dung của phần tử chứa số liệu
        emptyCountElement.textContent = number_empty;
        document.getElementById('take').style.display = 'inline';
        document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
        if (number < 7)
        {
        document.getElementById('store').style.display = 'inline'; // Hiển thị lại nút "GIỮ ĐỒ"
        }
        else
        {
        document.getElementById('store').style.display = 'none'; // Hiển thị lại nút "GIỮ ĐỒ"
        displayMessage_global("Đã hết tủ trống!", "white");
        }
        console.log("Chuỗi '" + "start_count_" + "' được tìm thấy trong chuỗi");
    }
    // Nhận message max image
    else if (message.payloadString.includes("max_image")) {
        console.log("Chuỗi '" + "max_image" + "' được tìm thấy trong chuỗi");
        // Sử dụng regular expression để tìm số tự nhiên trong chuỗi
        document.getElementById('store').style.display = 'none'; // Ẩn nút GIỮ ĐỒ
        document.getElementById('get').style.display = 'inline'; // Hiển thị nút LẤY ĐỒ
        removeMessageElement();
        displayMessage_global("Đã hết tủ trống!", "white");
    }
    // Nhận message đã lưu image thành công, lấy id trong message và gửi đến esp32-cam
    else if (message.payloadString.includes("save_image_")) {
        
        removeMessageElement();
        console.log("Chuỗi '" + "save_image_" + "' được tìm thấy trong chuỗi '" + message.payloadString + "'");
        // Sử dụng regular expression để tìm số tự nhiên trong chuỗi
        var matches = message.payloadString.match(/\d+/);
        if (matches) {
        number = parseInt(matches[0]);
        }
        document.getElementById('store').style.display = 'none'; // Ẩn nút LẤY ĐỒ
        document.getElementById('get').style.display = 'none'; // Ẩn nút LẤY ĐỒ
        status_back_finger = true;
        document.getElementById('back').style.display = 'inline'; // Hiển thị nút "Trở lại"
        displayMessage_global("Vui lòng nhập vân tay...", "#C8C905");
        publishToDifferentTopic(messageToSend + number, topicToSend);
    }
    // Nhận message đã lưu finger thành công và hiện lại các option
    else if (message.payloadString.includes("save_finger_ok")) {
        var matches = message.payloadString.match(/\d+/);
        console.log("matches_" + matches);
        if (matches) {
        number = parseInt(matches[0]); 
        }
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        removeMessageElement();
        console.log("Chuỗi '" + "save_finger_ok" + "' được tìm thấy trong chuỗi '" + message.payloadString + "'");
        displayMessage("Tủ số " + number + " đã mở!", 0, displayMessageTimeout, "white");
        setTimeout(() => {
            document.getElementById('store').style.display = 'inline'; // Hiển thị lại nút "GIỮ ĐỒ"
            document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
            // Cập nhật nội dung của phần tử chứa số liệu
            number_empty = number_empty - 1;
            emptyCountElement.textContent = number_empty;
        }, displayMessageTimeout);
    }
    // Nhận message đã detect được khuôn mặt người và hiện lại các option
    else if (message.payloadString.includes("image_detect_")) {
        var matches = message.payloadString.match(/\d+/);
        console.log("matches_" + matches);
        if (matches) {
        number = parseInt(matches[0]); 
        }
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        removeMessageElement();
        console.log("Chuỗi '" + "image_detect_" + "' được tìm thấy trong chuỗi '");
        displayMessage("Cửa số " + number + " đã mở!", 0, displayMessageTimeout, "white");
        setTimeout(() => {
            document.getElementById('store').style.display = 'inline'; // Hiển thị lại nút "GIỮ ĐỒ"
            document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
            // Cập nhật nội dung của phần tử chứa số liệu
            number_empty = number_empty + 1;
            emptyCountElement.textContent = number_empty;
        }, displayMessageTimeout);
        get_item = false;
        status_ = false;
    }
    // Nhận message đã detect được khuôn mặt người và hiện lại các option (case nhiều khuôn mặt)
    else if (message.payloadString.includes("detect_")) {
        // Tìm tất cả các số trong chuỗi sử dụng regex
        var numbers = message.payloadString.match(/\d+/g);
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        // Gán các số vào một chuỗi mới
        var resultString = numbers.join(', ');
        removeMessageElement();
        console.log("Chuỗi '" + "detect_" + "' được tìm thấy trong chuỗi '");
        displayMessage("Tủ số " + resultString + " đã mở!", 0, displayMessageTimeout, "white");
        setTimeout(() => {
            document.getElementById('store').style.display = 'inline'; // Hiển thị lại nút "GIỮ ĐỒ"
            document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
            // Cập nhật nội dung của phần tử chứa số liệu
            number_empty = number_empty + numbers.length;
            emptyCountElement.textContent = number_empty;
        }, displayMessageTimeout);
        get_item = false;
        status_ = false;
    }
    // Nhận message không tìm thấy khuôn mặt và gửi message đến esp32-cam để start compare fingerprint
    else if (message.payloadString.includes("no_person_fingerprint")) {
        removeMessageElement();
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        console.log("Chuỗi '" + "no_person_fingerprint" + "' được tìm thấy trong chuỗi '");
        displayMessage("Không có thông tin. Vui lòng thử lại.", 0, displayMessageTimeout, "white");
        setTimeout(() => {
            document.getElementById('store').style.display = 'inline'; // Hiển thị lại nút "GIỮ ĐỒ"
            document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
        }, displayMessageTimeout);
        get_item = false;
        status_ = false;
    }
    // Nhận message không tìm thấy khuôn mặt và gửi message đến esp32-cam để start compare fingerprint
    else if (message.payloadString.includes("no_person_match")) {
        removeMessageElement();
        console.log("Chuỗi '" + "no_person_match" + "' được tìm thấy trong chuỗi '");
        status_back_finger = true;
        document.getElementById('back').style.display = 'inline'; // Hiển thị nút "Trở lại"
        displayMessage_global("Vui lòng nhập vân tay để xác minh...", "#C8C905");
        publishToDifferentTopic(comparefinger, topicToSend);
    }
    // Nhận message đã detect được khuôn mặt người và hiện lại các option
    else if (message.payloadString.includes("old_image_")) {
        var matches = message.payloadString.match(/\d+/);
        console.log("matches_" + matches);
        if (matches) {
        number_old_image = parseInt(matches[0]); 
        }
        old_image = true;
        removeMessageElement();
        displayMessage_global("Bạn đã gửi đồ ở ô số " + number_old_image + ". Bạn có muốn gửi thêm ?", "#C8C905");
        document.getElementById('back').style.display = 'inline'; // Hiển thị nút "Trở lại"
        document.getElementById('download').style.display = 'inline'; // Hiển thị nút "Tiếp tục"
        console.log("Chuỗi '" + "old_image_" + "' được tìm thấy trong chuỗi '");
    }
    else if (message.payloadString.includes("continue")) {
        console.log("Chuỗi '" + "continue" + "' được tìm thấy trong chuỗi '");
        publishToDifferentTopic(message_continue_backend, topicToSend_py);
        removeMessageElement();
        displayMessage("Đóng tủ số " + number_old_image + ".", 0, displayMessageTimeout, "white");
        setTimeout(() => {
        if (number_empty > 0) {
            document.getElementById('store').style.display = 'inline'; // Hiển thị nút "gửi đồ"
        }
        document.getElementById('get').style.display = 'inline'; // Hiển thị lại nút "LẤY ĐỒ"
        }, displayMessageTimeout);
    }
    else {
        console.log("Không chuỗi nào" + "' được tìm thấy trong chuỗi '");
    }
}
function on_load()
{
    document.getElementById('take').style.display = 'none';
    document.getElementById('fail').style.display = 'none';
    console.log("on_load");
}

function on_close(event)
{
    console.log("on_close_connection");
    // Kiểm tra xem có thể thực hiện kết nối lại hay không
    if (event.wasClean) {
    console.log(`Kết nối đã đóng clean, mã: ${event.code}, lý do: ${event.reason}`);
    } else {
    console.error('Kết nối đã bị đóng không mong muốn');
    }
    // Thực hiện kết nối lại sau một khoảng thời gian (ví dụ: sau 3 giây)
    setTimeout(connectWebSocket, 1000);
}

function on_open()
{
    console.log("on_open_connection");
    // setTimeout(connectWebSocket, 3000);
}

function connectWebSocket()
{
    var text = document.getElementById('connect_input');
    var connection = "ws://" + text.value + ":80/ws";
    web_socket = new WebSocket(connection);
    web_socket.binaryType = 'arraybuffer';
    web_socket.onmessage = on_message;
    web_socket.onopen = on_open;
    web_socket.onerror = on_error;
    web_socket.onclose = on_close;  // Thêm sự kiện xử lý khi kết nối đóng
}

function on_connect_server()
{
    document.getElementById('take').style.display = 'none';
    document.getElementById('connect').style.display = 'none';
    connectWebSocket();  // Gọi hàm để kết nối WebSocket
    console.log("on_connect_server");
}

function on_store_item()
{
    status_back = false;
    status_back_finger = false;
    store_item = true;
    web_socket.send(text_input);
}

function on_get_item()
{
    status_back = false;
    status_back_finger = false;
    get_item = true;
    removeMessageElement();
    web_socket.send(text_input);
}

function on_download_image()
{
    if (old_image == false) {
    count_print_message = 0;
    console.log("on_download_image");
    document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
    document.getElementById('download').style.display = 'none'; // Ẩn nút "Tiếp tục"
    removeMessageElement();
    if (true == store_item) {
        // Gọi hàm để thực hiện tải xuống hình ảnh
        downloadImage(canvas_image.toDataURL('image/jpeg'), "image_store.jpg");
        displayMessage_global("Hình ảnh của bạn đang được lưu trữ.", "white");
        store_item = false;
    }
    else if (true == get_item) {
        // Gọi hàm để thực hiện tải xuống hình ảnh
        downloadImage(canvas_image.toDataURL('image/jpeg'), "image_get.jpg");
        displayMessage_global("Thông tin của bạn đang được xử lý.", "white");
        get_item = false;
    }
    clearBoundingBoxes();
    faceDetectionCount = 0;
    end_image = false;
    }
    else {
    old_image = false;
    removeMessageElement();
    document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
    document.getElementById('download').style.display = 'none'; // Ẩn nút "Tiếp tục"
    publishToDifferentTopic(message_old_image + number_old_image, topicToSend);
    displayMessage_global("Tủ số " + number_old_image + " mở trong 11s để bạn gửi thêm đồ!", "white");

    }
}

function on_back()
{
    if (old_image == false) {
    if (status_back_finger == false)
    {
        status_back = true;
        if (status_image == false)
        {
        console.log("status_image is true");
        // Gửi thông tin đã lưu image đến esp32-cam
        // var text_input_1 = "downloaded_image";
        // web_socket.send(text_input_1);
        publishToDifferentTopic(downloaded_image, topicToSend);
        }
        count_print_message = 0;
        console.log("on_back");
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        document.getElementById('download').style.display = 'none'; // Ẩn nút "Tiếp tục"
        removeMessageElement();
        clearBoundingBoxes();
        faceDetectionCount = 0;
        if (number_empty > 0) {
        document.getElementById('store').style.display = 'inline'; // Hiển thị nút "gửi đồ"
        }
        document.getElementById('get').style.display = 'inline'; // Hiển thị nút "Lấy đồ"
        status_image = false;
    }
    else 
    {
        removeMessageElement();
        publishToDifferentTopic("back_finger", topicToSend);
        document.getElementById('back').style.display = 'none'; // Ẩn nút "Trở lại"
        if (number_empty > 0) {
        document.getElementById('get').style.display = 'inline'; // Hiển thị nút "lấy đồ"
        }
        document.getElementById('store').style.display = 'inline'; // Hiển thị nút "gửi đồ"
        status_back_finger = false;
    }
    end_image = false;
    }
    else {
    old_image = false;
    removeMessageElement();
    publishToDifferentTopic(message_continue_backend, topicToSend_py);
    document.getElementById('back').style.display = 'none'; // ẩn nút "Trở lại"
    document.getElementById('download').style.display = 'none'; // Ẩn nút "Tiếp tục"
    if (number_empty > 0) {
        document.getElementById('get').style.display = 'inline'; // Hiển thị nút "lấy đồ"
    }
    document.getElementById('store').style.display = 'inline'; // Hiển thị nút "gửi đồ"
    }
}

function on_error(recv_data)
{
    console.log("on_error");
    document.getElementById('take').style.display = 'none';
    document.getElementById('fail').style.display = 'inline';
    removeMessageElement();
}

function on_close_connection() {
    console.log("on_close_connection");
    if (web_socket !== null) {
        web_socket.close();
        web_socket = null;
    }
    removeMessageElement();
}
function publishToDifferentTopic(message, topic) {
    var message = new Paho.MQTT.Message(message);
    message.destinationName = topic;
    client.send(message);
}
async function on_message(recv_data)
{
    // console.log(recv_data.data);
    // console.log("on_message");
    //Get image with "Giữ Đồ" option, display it and save image if the face is detected
    if ( ((true == store_item) || (true == get_item)) && (status_back == false) && (end_image == false))
    {
    document.getElementById('back').style.display = 'inline'; // Hiển thị nút "Trở lại"
    document.getElementById('store').style.display = 'none'; // Ẩn nút LẤY ĐỒ
    document.getElementById('get').style.display = 'none'; // Ẩn nút LẤY ĐỒ
    faceapi.tf.engine().startScope();
    var recv_image_data = new Uint8Array(recv_data.data);
    var canvas_image = document.getElementById('canvas_image');
    canvas_image.style.marginTop = '10px';
    var ctx = canvas_image.getContext('2d');
    ctx.globalCompositeOperation = 'source-over';
    var image = new Image();
    image.src = 'data:image/jpeg;base64,' + recv_data.data;
    image.onload = async function() {
        ctx.drawImage(image, 0, 0, 640*0.75, 480*0.65);
        // Canh canvas vào giữa trang
        canvas_image.style.left = '-13%';
        canvas_image.style.transform = 'translate(13%)';
        await faceapi.nets.tinyFaceDetector.loadFromUri('./models');
        const detection = await faceapi.detectSingleFace(canvas_image, new faceapi.TinyFaceDetectorOptions({ inputSize: 160, scoreThreshold: 0.7}));
        if (faceDetectionCount < count_image - 3 && detection !== undefined) {
            const { x, y, width, height } = detection.box;
            ctx.beginPath();
            ctx.strokeStyle = '#37D3D4';
            ctx.rect(x, y, width, height);
            ctx.lineWidth = 2;
            ctx.stroke();
            ctx.closePath();
        }
        if( detection !== undefined) {
            // Tăng biến đếm khi có khuôn mặt được phát hiện
            if (!faceDetectedLastTime) {
                // Nếu có phát hiện khuôn mặt sau khi không phát hiện ở lần trước
                // Thì đặt lại biến đếm về 1
                faceDetectionCount = 0;
            } else {
                // Nếu vẫn phát hiện khuôn mặt liên tục
                faceDetectionCount++;
            }
            faceDetectedLastTime = true;
        } else {
            // Nếu không phát hiện khuôn mặt
            faceDetectionCount = 0;
            faceDetectedLastTime = false;
        }

        // Kiểm tra nếu có 10 lần phát hiện liên tiếp
        if (faceDetectionCount >= count_image) {
        document.getElementById('back').style.display = 'inline'; // hiển thị nút "Trở lại"
        document.getElementById('download').style.display = 'inline'; // hiển thị nút "Tiếp tục"
        status_image = true;
        count_print_message = count_print_message + 1;
        if (count_print_message == 1)
        {
            displayMessage_global_image("Bạn có muốn tiếp tục với hình ảnh bên dưới?", "#C8C905");
        }
        console.log("Downloaded image!!!");
        // Gửi thông tin đã lưu image đến esp32-cam
        publishToDifferentTopic(downloaded_image, topicToSend);
        end_image = true;
        }
    }
    // Dispose WebGLTexture
    faceapi.tf.engine().endScope();
    await faceapi.nets.tinyFaceDetector.dispose();
    }
}

function clearBoundingBoxes() {
    var canvas = document.getElementById('canvas_image');
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    // Xóa toàn bộ canvas
    ctx.globalCompositeOperation = 'destination-out';
    // ctx.fillStyle = 'transparent';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
}

// Hàm để tải xuống hình ảnh
function downloadImage(dataUrl, file_name) {
    // Tạo một đối tượng a để tạo link tải xuống
    var a = document.createElement('a');
    a.href = dataUrl;
    a.download = file_name; // Đặt tên cho file tải xuống
    document.body.appendChild(a);
    a.click(); // Kích hoạt sự kiện click trên đối tượng a
    document.body.removeChild(a);
}

function displayMessage(message, no_timeout, timeout, string_color)
{
    var messageElement = document.createElement('div');
    messageElement.style.fontFamily = "'Bahnschrift SemiBold', sans-serif";
    messageElement.style.fontSize = '1.6vw';
    messageElement.style.fontWeight = 'bold';
    messageElement.style.color = string_color;
    messageElement.style.borderRadius = '1vw'; // Đặt bán kính bo tròn cho góc của background
    messageElement.style.backgroundColor = '#2B3252';
    messageElement.style.position = 'absolute';
    messageElement.style.top = '42%';
    messageElement.style.left = '50%';
    messageElement.style.display = 'flex';
    messageElement.style.padding = '0.5vw';
    messageElement.style.transform = 'translate(-50%, -42%)';
    // messageElement.style.maxWidth = '80vw'; // Đặt chiều rộng tối đa cho thông báo
    // messageElement.style.maxHeight = '80vh'; // Đặt chiều cao tối đa cho thông báo
    // messageElement.style.overflow = 'auto'; // Bật thanh cuộn khi thông báo quá lớn
    // messageElement.style.margin = '1vw';
    messageElement.innerHTML = message;

    document.body.appendChild(messageElement);
    if (no_timeout == 0)
    {
    setTimeout(() => {
        document.body.removeChild(messageElement);
    }, timeout);
    }
}

function displayMessage_global(message, string_color)
{
    var messageElement = document.createElement('div');
    messageElement.style.fontFamily = "'Bahnschrift SemiBold', sans-serif";
    messageElement.style.fontSize = '1.6vw';
    messageElement.style.fontWeight = 'bold';
    messageElement.style.color = string_color;
    messageElement.style.borderRadius = '1vw'; // Đặt bán kính bo tròn cho góc của background
    messageElement.style.backgroundColor = '#2B3252';
    messageElement.style.position = 'absolute';
    messageElement.style.top = '42%';
    messageElement.style.left = '50%';
    messageElement.style.display = 'flex';
    messageElement.style.padding = '0.5vw';
    messageElement.style.transform = 'translate(-50%, -42%)';
    // messageElement.style.maxWidth = '80vw'; // Đặt chiều rộng tối đa cho thông báo
    // messageElement.style.maxHeight = '80vh'; // Đặt chiều cao tối đa cho thông báo
    // messageElement.style.overflow = 'auto'; // Bật thanh cuộn khi thông báo quá lớn
    // messageElement.style.margin = '1vw';
    messageElement.innerHTML = message;

    document.body.appendChild(messageElement);
    globalMessageElement = messageElement;
}

function displayMessage_global_image(message, string_color)
{
    var messageElement = document.createElement('div');
    messageElement.style.fontFamily = "'Bahnschrift SemiBold', sans-serif";
    messageElement.style.fontSize = '1.6vw';
    messageElement.style.fontWeight = 'bold';
    messageElement.style.color = string_color;
    messageElement.style.borderRadius = '1vw'; // Đặt bán kính bo tròn cho góc của background
    messageElement.style.backgroundColor = '#2B3252';
    messageElement.style.position = 'absolute';
    messageElement.style.top = '42%';
    messageElement.style.left = '50%';
    messageElement.style.display = 'flex';
    messageElement.style.padding = '0.5vw';
    messageElement.style.transform = 'translate(-50%, -42%)';
    // messageElement.style.maxWidth = '80vw'; // Đặt chiều rộng tối đa cho thông báo
    // messageElement.style.maxHeight = '80vh'; // Đặt chiều cao tối đa cho thông báo
    // messageElement.style.overflow = 'auto'; // Bật thanh cuộn khi thông báo quá lớn
    // messageElement.style.margin = '1vw';
    messageElement.innerHTML = message;

    document.body.appendChild(messageElement);
    globalMessageElement = messageElement;
}

function removeMessageElement()
{
    if (globalMessageElement && globalMessageElement.parentNode) {
    // console.log("globalMessageElement", globalMessageElement);
    globalMessageElement.parentNode.removeChild(globalMessageElement);
    }
}