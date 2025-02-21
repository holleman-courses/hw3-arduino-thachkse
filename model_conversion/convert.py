import tensorflow as tf

# Load the Keras model
model = tf.keras.models.load_model('sin_predictor.h5')

# Convert to TensorFlow Lite model
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

# Save the TFLite model
with open('sine_model.tflite', 'wb') as f:
    f.write(tflite_model)
