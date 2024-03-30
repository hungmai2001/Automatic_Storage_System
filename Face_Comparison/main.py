import os
import time
import folder_process as f1
import mongoDB_MQTT as f2
from pymongo.mongo_client import MongoClient
from gridfs import GridFS
import paho.mqtt.publish as publish
import re

max_number_image = 7
number_image = {}
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
        global number_image
        """Thêm một số vào dãy số."""
        number_image[int(number)] = 1
        # self.sequence.append(number)
    def remove_number(self, number):
        global number_image
        """Xóa một số khỏi dãy số."""
        print(f"number_image: {number_image}")
        if number in number_image:
            del number_image[number]
        else:
            print(f"Số {number} không tồn tại trong dãy.")
    def display_sequence(self):
        """Hiển thị dãy số."""
        print("Dãy số hiện tại:")
        global number_image
        for number in number_image: 
            print(number, end=" ")
    def send_mqtt(self):
        """Gửi dãy số đến mqtt"""
        #Send
    def choose_number(self):
        global number_image
        all_numbers = set(range(1, max_number_image + 1))  # Tạo tất cả các số có thể
        print(f"number_image: {number_image}")
        print(f"all_numbers: {all_numbers}")
        print(f"number_image.keys(): {number_image.keys()}")
        print(f"set-number_image.keys(): {set(number_image.keys())}")
        available_numbers = all_numbers - set(number_image.keys())  # Loại bỏ các số đã có trong biến global number_image
        print(f"available_numbers: {available_numbers}")
        if available_numbers:  # Kiểm tra xem có số nào khả dụng không
            return min(available_numbers)  # Trả về số nhỏ nhất trong các số khả dụng
        else:
            return 0  # Nếu không còn số khả dụng, trả về 0
    def count_number(self):
        count = 0
        """Đếm số"""
        global number_image
        for number in number_image:
            count = count + 1
        return count
# Tạo một đối tượng quản lý dãy số
manager = NumberSequenceManager()

# Đếm số lượng, lấy số của hình ảnh và return về danh sách
def images_in_folder(folder_store, name_image):
    path = "start_count_"
    count = f1.count_images_in_folder(folder_store)
    path = path + f"{count}_num"
    image_files = os.listdir(folder_store)
    for file in image_files:
        matches = re.findall(f'{name_image}(\d+)', file)
        for match in matches:
            path = path + f"_{match}"
            manager.add_number(match)
    print("path to send mqtt: " + path)
    return path

if __name__ == "__main__":
    # Xóa ảnh trong store folder
    f1.remove_file_in_folder(folder_store)
    # Clone image từ database
    f2.get_image_from_database(fs, db, folder_store)
    # # Đếm số lượng hình ảnh trong store folder
    # count = f1.count_images_in_folder(folder_store)
    # Publish đến MQTT
    # publish.single(topic, f"start_count_{count}", hostname=broker_address)
    path_send = images_in_folder(folder_store, name_image)
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
                manager.display_sequence()
                image_count = manager.choose_number()
                # manager.remove_number(8)
                publish.single(topic,f"save_image_{image_count}", hostname=broker_address)
                start_finger = f2.loop_mqtt_to_waiting_events_start()
                if (start_finger == "store_fingerprint"):
                    new_file_move = f1.move_image(file_check, folder_to_check, folder_store, fs, image_count)
                    if new_file_move:
                        count = f1.count_images_in_folder(folder_store)
                        manager.add_number(image_count)
                        publish.single(topic,"save_finger_ok", hostname=broker_address)
                        # publish.single(topic,f"save_image_{count}", hostname=broker_address)

                        #Loop để chờ data finger từ esp32-cam
                        #####
                        # Lưu finger ứng với hình ảnh new_file_move này (có stt trong này)

                        # Publish infor tới web browser là đã xử lý xong
                    else:
                        print("Error")
            elif "image_get" in file_check:
                number_img = f1.find_unknown_people(file_check, folder_store, folder_to_check, name_image, fs)
                print(f"number_img: {number_img}")
                s = int(number_img)
                if s > 0:
                    manager.remove_number(s)
                    print("Remove ok")
            count = manager.count_number()
            if (count >= max_number_image):
                time.sleep(5)
                publish.single(topic,"max_image", hostname=broker_address)
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
