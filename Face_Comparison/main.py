import os
import time
import folder_process as f1
import mongoDB_MQTT as f2
from pymongo.mongo_client import MongoClient
from gridfs import GridFS
import paho.mqtt.publish as publish
import re

max_number_image = 7
name_image = "image_store_"
# Đường dẫn đến thư mục cần kiểm tra
folder_to_check = "D:/DATN/esp32_cam_image"
# Tên của tệp cần kiểm tra
name_to_check = "image"
# Thư mục để lưu trữ khuôn mặt cần lưu
folder_store = "D:/DATN/esp32_cam_image/img_store"
# Tạo thư mục nếu nó không tồn tại
os.makedirs(folder_store, exist_ok=True)

#MongoDB database
uri = "mongodb+srv://hungmai2001:Xuanhung123@cluster0.3gjdvmd.mongodb.net/?retryWrites=true&w=majority"
# Create a new client and connect to the server
client = MongoClient(uri)
# Chọn hoặc tạo một database mới
db = client["MongoDB_image"]
# Chọn hoặc tạo GridFS
fs = GridFS(db, collection="images")

# MQTT
broker_address = "mqtt.eclipseprojects.io"
topic = "/topic/hungmai_py_publish"
topic_esp = "/topic/hungmai_wb_publish"

class NumberSequenceManager:
    def __init__(self):
        self.sequence = []  # Danh sách lưu trữ dãy số
    def add_number(self, number):
        """Thêm một số vào dãy số."""
        self.sequence.append(number)
    def remove_number(self, number):
        """Xóa một số khỏi dãy số."""
        if number in self.sequence:
            self.sequence.remove(number)
        else:
            print(f"Số {number} không tồn tại trong dãy.")
    def display_sequence(self):
        """Hiển thị dãy số."""
        print("Dãy số hiện tại:")
        for number in self.sequence:
            print(number, end=" ")
    def send_mqtt(self):
        """Gửi dãy số đến mqtt"""
        #Send
    def choose_number(self):
        for i in range(max_number_image):
            if i not in self.sequence:
                return i
        return 0
        #Send


# Tạo một đối tượng quản lý dãy số
manager = NumberSequenceManager()

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

if __name__ == "__main__":
    # Xóa ảnh trong store folder
    f1.remove_file_in_folder(folder_store)
    # Clone image từ database
    f2.get_image_from_database(fs, db, folder_store)
    # # Đếm số lượng hình ảnh trong store folder
    # count = f1.count_images_in_folder(folder_store)
    # Publish đến MQTT
    # publish.single(topic, f"start_count_{count}", hostname=broker_address)
    path_send = f1.images_in_folder(folder_store, name_image)
    publish.single(topic, path_send, hostname=broker_address)
    # Vòng lặp vô hạn
    while (True):
        file_check = f1.check_file_in_folder(folder_to_check, name_to_check)
        if file_check:
            if "image_store" in file_check:
                # Sử dụng regular expression để tìm số tự nhiên trong chuỗi
                # match = re.search(r'\d+', file_check)
                # if match:
                #     # Nếu tìm thấy số tự nhiên, lấy giá trị và chuyển đổi thành số nguyên
                #     x = int(match.group())

                # filepath = os.path.join(folder_to_check, file_check)
                new_file_move = f1.move_image(file_check, folder_to_check, folder_store, fs)
                if new_file_move:
                    count = f1.count_images_in_folder(folder_store)
                    publish.single(topic,f"save_image_{count}", hostname=broker_address)

                    #Loop để chờ data finger từ esp32-cam
                    #####
                    # Lưu finger ứng với hình ảnh new_file_move này (có stt trong này)

                    # Publish infor tới web browser là đã xử lý xong
                else:
                    print("Error")
            elif "image_get" in file_check:
                f1.find_unknown_people(file_check, folder_store, folder_to_check, name_image, fs)
        # Chờ 2 giây trước khi kiểm tra lại
        else:
            time.sleep(2)
    # file_path = "D:/DATN/esp32_cam_image/img_get/image_store_2.jpg"
    # file_path1 = "D:/DATN/esp32_cam_image/img_get/image_store_4.jpg"
    # file_path2 = "D:/DATN/esp32_cam_image/img_get/image_store_6.jpg"
    # file_path3 = "D:/DATN/esp32_cam_image/img_get/image_store_7.jpg"
    # # f2.push_image(file_path, fs, "image_store_2.jpg")
    # # f2.push_image(file_path1, fs, "image_store_4.jpg")
    # f2.push_image(file_path2, fs, "image_store_6.jpg")
    # f2.push_image(file_path3, fs, "image_store_7.jpg")
