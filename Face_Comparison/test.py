# import asyncio
# import websockets


# async def send_signal(signal):
#     uri = "ws://192.168.1.11:80/ws_py"
#     try:
#         async with websockets.connect(uri) as websocket:
#             if websocket.open:
#             # Gửi tín hiệu đến server
#                 await websocket.send(signal)
#                 print(f"Sent signal: {signal}")
#             else:
#                 print("Failed to connect to WebSocket server.")
#     except Exception as e:
#         print(f"An error occurred: {e}")
#     finally:
#         # Đảm bảo đóng kết nối sau khi gửi xong
#         if websocket and websocket.open:
#             await websocket.close()
#         print("WebSocket connection closed.")

# asyncio.get_event_loop().run_until_complete(send_signal("image_get_1"))

# Test HTTP response
# import requests

# # URL của HTTP server
# url = 'http://192.168.1.8/pyhttp'

# # Dữ liệu bạn muốn gửi (ở đây là một dictionary)
# data = {'key1': 'value1', 'key2': 'value2'}

# # Gửi request POST đến server với dữ liệu
# response = requests.post(url, data=data)

# # Kiểm tra kết quả của request
# if response.status_code == 200:
#     print('Dữ liệu đã được gửi thành công!')
# else:
#     print('Có lỗi xảy ra khi gửi dữ liệu:', response.status_code)

import paho.mqtt.client as mqtt

# # Define callback function for when a message is received
# def on_message(client, userdata, message):
#     print("Received message:", message.payload.decode())

# # Create MQTT client instance
# client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

# # Set callback function
# client.on_message = on_message

# # Connect to MQTT broker
# broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
# client.connect(broker_address)

# # Subscribe to topic
# topic = "/topic/qos0"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
# client.subscribe(topic)

# # Start the MQTT loop to listen for messages
# client.loop_forever()

# import paho.mqtt.publish as publish

# # Thiết lập thông tin của broker MQTT
# broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
# topic = "/topic/qos0"  # Thay bằng tên chủ đề MQTT bạn muốn gửi tin nhắn đến

# import re

# # Chuỗi đại diện cho tên file
# file_check = "image_store_1.png"

# # Sử dụng regular expression để tìm số tự nhiên trong chuỗi
# match = re.search(r'\d+', file_check)

# if match:
#     # Nếu tìm thấy số tự nhiên, lấy giá trị và chuyển đổi thành số nguyên
#     x = int(match.group())
#     print("Số tự nhiên x:", x)
# else:
#     print("Không tìm thấy số tự nhiên trong chuỗi")

# Define callback function for when a message is received
# def on_message(client, userdata, message):
#     print("Received message:", message.payload.decode())
#     if (message.payload.decode() == "start"):
#         print("OK")
#     global message_received
#     message_received = True

# def loop_mqtt_to_waiting_events():
#     global message_received
#     message_received = False

#     # Create MQTT client instance
#     client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
#     # Set callback function
#     client.on_message = on_message
#     # Connect to MQTT broker
#     broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
#     client.connect(broker_address)
#     # Subscribe to topic
#     topic = "/topic/hungmai_finger_result"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
#     client.subscribe(topic)

#     # Loop until a message is received
#     while not message_received:
#         client.loop()

#     # Disconnect MQTT client
#     client.disconnect()
# loop_mqtt_to_waiting_events()

# import re

# # Chuỗi ban đầu
# string = "abc123def456ghi"

# # Sử dụng regular expression để tìm số duy nhất trong chuỗi
# match = re.search(r'\d+', string)

# # Kiểm tra xem có số nào được tìm thấy không
# if match:
#     # Lấy số đầu tiên được tìm thấy
#     number = int(match.group())
#     print("Số duy nhất trong chuỗi:", number)
# else:
#     print("Không tìm thấy số trong chuỗi.")

# class NumberSequenceManager:
#     def __init__(self):
#         self.sequence = []  # Danh sách lưu trữ dãy số

#     def add_number(self, number):
#         """Thêm một số vào dãy số."""
#         self.sequence.append(number)

#     def remove_number(self, number):
#         """Xóa một số khỏi dãy số."""
#         if number in self.sequence:
#             self.sequence.remove(number)
#         else:
#             print(f"Số {number} không tồn tại trong dãy.")

#     def display_sequence(self):
#         """Hiển thị dãy số."""
#         print("Dãy số hiện tại:")
#         for number in self.sequence:
#             print(number, end=" ")
#         print()

# # Tạo một đối tượng quản lý dãy số
# manager = NumberSequenceManager()

# # Thêm các số vào dãy số
# manager.add_number(1)
# manager.add_number(2)
# manager.add_number(3)

# # Hiển thị dãy số
# manager.display_sequence()

# # Xóa một số khỏi dãy số
# manager.remove_number(2)

# # Hiển thị dãy số sau khi xóa
# manager.display_sequence()


