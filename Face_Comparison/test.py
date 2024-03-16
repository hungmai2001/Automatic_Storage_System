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

# Define callback function for when a message is received
def on_message(client, userdata, message):
    print("Received message:", message.payload.decode())

# Create MQTT client instance
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

# Set callback function
client.on_message = on_message

# Connect to MQTT broker
broker_address = "mqtt.eclipseprojects.io"  # Thay bằng địa chỉ broker MQTT của bạn
client.connect(broker_address)

# Subscribe to topic
topic = "/topic/qos0"  # Thay bằng tên chủ đề MQTT bạn muốn subscribe
client.subscribe(topic)

# Start the MQTT loop to listen for messages
client.loop_forever()
