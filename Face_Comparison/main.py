import os
import time
import folder_process as f1
import mongoDB as f2
from pymongo.mongo_client import MongoClient
from gridfs import GridFS
import paho.mqtt.publish as publish

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

if __name__ == "__main__":
    # Xóa ảnh trong store folder
    f1.remove_file_in_folder(folder_store)
    # Clone image từ database
    f2.get_image_from_database(fs, db, folder_store)
    # Đếm số lượng hình ảnh trong store folder
    count = f1.count_images_in_folder(folder_store)
    # Publish đến MQTT
    publish.single(topic, f"start_count_{count}", hostname=broker_address)
    # Vòng lặp vô hạn
    while (True):
        file_check = f1.check_file_in_folder(folder_to_check, name_to_check)
        if file_check:
            if "image_store" in file_check:
                filepath = os.path.join(folder_to_check, file_check)
                new_file_move = f1.move_image(file_check, folder_to_check, folder_store)
                if new_file_move:
                    f2.push_image(new_file_move, fs, file_check)
                    publish.single(topic,"save_image_ok", hostname=broker_address)

                    #Loop để chờ data finger từ esp32-cam
                    #####
                    # Lưu finger ứng với hình ảnh new_file_move này (có stt trong này)

                    # Publish infor tới web browser là đã xử lý xong
                else:
                    print("Error")
            elif "image_get" in file_check:
                f1.find_unknown_people(file_check, folder_store, folder_to_check, name_image)
        # Chờ 2 giây trước khi kiểm tra lại
        else:
            time.sleep(2)
