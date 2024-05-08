import os
import paho.mqtt.client as mqtt
import re

def push_image(file_path, fs, file_name):
    with open(file_path, "rb") as f:
        # Lưu trữ hình ảnh trong GridFS
        fs.put(f, filename=file_name)
        print(f"Upload {file_name} completed.")

def get_image_from_database(fs,db,output_folder):
    # Lấy danh sách tất cả các hình ảnh từ GridFS
    image_list = list(fs.find())
    collections_list = db.list_collection_names()
    print("Danh sách các collections:", collections_list)
    print("get_image", image_list)
    # Lặp qua danh sách và lưu từng hình ảnh xuống thư mục img_get
    for image_info in image_list:
        image_id = image_info._id
        image_data = fs.get(image_id).read()
        image_name = image_info.filename
        # Tạo đường dẫn để lưu hình ảnh
        output_path = os.path.join(output_folder, image_info.filename)
        # Lưu hình ảnh xuống thư mục img_get
        with open(output_path, "wb") as output_file:
            output_file.write(image_data)

        print(f"Hình ảnh {image_name} đã được lưu tại {output_path}")

def delete_image(filename,fs):
    # Tìm thông tin về hình ảnh trong GridFS
    image_info = fs.find_one({"filename": filename})

    if image_info:
        image_id = image_info._id
        # Xóa hình ảnh từ GridFS
        fs.delete(image_id)
        print(f"Hình ảnh {filename} đã bị xóa thành công.")
    else:
        print(f"Không tìm thấy hình ảnh có tên {filename} trong GridFS.")

# Define callback function for when a message is received
def on_message(client, userdata, message):
    print("Received message:", message.payload.decode())
    # Sử dụng regular expression để tìm số duy nhất trong chuỗi
    match = re.search(r'\d+', message.payload.decode())
    global number
    # Kiểm tra xem có số nào được tìm thấy không
    if match:
        # Lấy số đầu tiên được tìm thấy
        number = int(match.group())
        print("Số duy nhất trong chuỗi:", number)
    else:
        number = -1
        print("Không tìm thấy số trong chuỗi.")

    global message_received
    message_received = True

def loop_mqtt_to_waiting_events():
    global message_received
    global number
    message_received = False
    # Create MQTT client instance
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    # Set callback function
    client.on_message = on_message
    # Connect to MQTT broker
    broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
    client.connect(broker_address)
    # Subscribe to topic
    topic = "/topic/hungmai_finger_result"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
    client.subscribe(topic)

    # Loop until a message is received
    while not message_received:
        client.loop()

    # Disconnect MQTT client
    client.disconnect()
    return number

# Define callback function for when a message is received
def on_message_start(client, userdata, message):
    global string_finger
    global message_received_
    message_received_ = False
    if "store_fingerprint" in message.payload.decode():
        string_finger = message.payload.decode()
        print("store_fingerprint được tìm thấy số trong chuỗi.")
        print(message.payload.decode())
    elif "fingerprint_end" in message.payload.decode():
        string_finger = message.payload.decode()
        print("fingerprint_end được tìm thấy số trong chuỗi.")
        print(message.payload.decode())
    else:
        string_finger = "None"
        print("None")
    message_received_ = True

def loop_mqtt_to_waiting_events_start():
    print("loop_mqtt_to_waiting_events_start")
    global message_received_
    message_received_ = False
    # Create MQTT client instance
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    # Set callback function
    client.on_message = on_message_start
    # Connect to MQTT broker
    broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
    client.connect(broker_address)
    # Subscribe to topic
    topic = "/topic/hungmai_finger_result"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
    client.subscribe(topic)

    # Loop until a message is received
    while not message_received_:
        client.loop()

    # Disconnect MQTT client
    client.disconnect()
    return string_finger


# Define callback function for when a message is received
def on_message_old_image(client, userdata, message):
    print("Received message:", message.payload.decode())
    global status_continue
    status_continue = False
    global message_received_x
    message_received_x = False
    if "continue" in message.payload.decode():
        print("Continue")
        print(message.payload.decode())
        status_continue = True
    else:
        print("None")
    message_received_x = True

def loop_mqtt_to_waiting_events_old_image():
    global message_received_x
    global status_continue
    message_received_x = False
    # Create MQTT client instance
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    # Set callback function
    client.on_message = on_message_old_image
    # Connect to MQTT broker
    broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
    client.connect(broker_address)
    # Subscribe to topic
    topic = "/topic/hungmai_py_subscribe"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
    client.subscribe(topic)

    # Loop until a message is received
    while not message_received_x:
        client.loop()

    # Disconnect MQTT client
    client.disconnect()
    return status_continue