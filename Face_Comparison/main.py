import os
import time
import folder_process as f1

name_image = "image_store_"
# Đường dẫn đến thư mục cần kiểm tra
folder_to_check = "D:/DATN/esp32_cam_image"
# Tên của tệp cần kiểm tra
name_to_check = "image"
# Thư mục để lưu trữ khuôn mặt cần lưu
folder_store = "D:/DATN/esp32_cam_image/img_store"
# Tạo thư mục nếu nó không tồn tại
os.makedirs(folder_store, exist_ok=True)


if __name__ == "__main__":
    # Vòng lặp vô hạn
    while (True):
        file_check = f1.check_file_in_folder(folder_to_check, name_to_check)
        if file_check:
            if "image_store" in file_check:
                f1.move_image(file_check, folder_to_check, folder_store)
            elif "image_get" in file_check:
                f1.find_unknown_people(file_check, folder_store, folder_to_check, name_image)
        # Chờ 2 giây trước khi kiểm tra lại
        else:
            time.sleep(2)
