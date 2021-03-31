#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
"""
Copyright (C) 2019. Huawei Technologies Co., Ltd. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the Apache License Version 2.0.You may not use this
file except in compliance with the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the Apache
License for more details at
    http://www.apache.org/licenses/LICENSE-2.0

This is a sample script that guides users how to apply AMCT toolkit to
classification network.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import math
import argparse
import numpy as np
import tensorflow as tf # pylint: disable=E0401
import matplotlib.pyplot as plt # pylint: disable=E0401
from tqdm import trange # pylint: disable=E0401

import amct_tensorflow as amct # pylint: disable=E0401
from amct_tensorflow.utils.log import LOGGER # pylint: disable=E0401
amct.set_logging_level(print_level='info', save_level='info')


SIZE = 224
MAX_RESIZE = 512
MIN_RESIZE = 256
_R_MEAN = 123.68
_G_MEAN = 116.78
_B_MEAN = 103.94


def _decode(item):
    features = {
        'image/encoded': tf.io.FixedLenFeature(
            (), tf.string, default_value=''),
        'image/format': tf.io.FixedLenFeature(
            (), tf.string, default_value='jpeg'),
        'image/class/label': tf.io.FixedLenFeature(
            [], dtype=tf.int64, default_value=-1),
        'image/class/text': tf.io.FixedLenFeature(
            [], dtype=tf.string, default_value=''),
        'image/object/bbox/xmin': tf.io.VarLenFeature(dtype=tf.float32),
        'image/object/bbox/ymin': tf.io.VarLenFeature(dtype=tf.float32),
        'image/object/bbox/xmax': tf.io.VarLenFeature(dtype=tf.float32),
        'image/object/bbox/ymax': tf.io.VarLenFeature(dtype=tf.float32),
        'image/object/class/label': tf.io.VarLenFeature(dtype=tf.int64)}

    example = tf.io.parse_single_example(item, features)
    image = example['image/encoded']
    label = example['image/class/label']
    text = example['image/class/text']
    image = tf.cond(tf.image.is_jpeg(image),
                    lambda: tf.image.decode_jpeg(image),
                    lambda: tf.image.decode_png(image))
    return image, label, text


def _grayscale_to_rgb(image):
    rgb_image = tf.cond(tf.equal(tf.shape(image)[-1], 1),
                        true_fn=lambda: tf.image.grayscale_to_rgb(image),
                        false_fn=lambda: image)
    return rgb_image


def _resize(image, resize_side):
    shape = tf.shape(image)
    height = tf.cast(shape[0], tf.float32)
    width = tf.cast(shape[1], tf.float32)
    scale = tf.cond(tf.greater(height, width),
                    lambda: resize_side / width,
                    lambda: resize_side / height)
    new_height = tf.cast(tf.math.rint(height * scale), tf.int32)
    new_width = tf.cast(tf.math.rint(width * scale), tf.int32)
    resized_image = tf.compat.v1.image.resize_images(
        image, [new_height, new_width])
    return resized_image


def _crop(image, is_random=False):
    shape = tf.shape(image)
    height = shape[0]
    width = shape[1]

    if is_random:
        max_offset_height = height - SIZE + 1
        max_offset_width = width - SIZE + 1
        offset_height = tf.random.uniform(
            [], maxval=max_offset_height, dtype=tf.int32)
        offset_width = tf.random.uniform(
            [], maxval=max_offset_width, dtype=tf.int32)
        cropped_image = tf.image.crop_to_bounding_box(
            image, offset_height, offset_width, SIZE, SIZE)
    else:
        offset_height = tf.cast((height - SIZE) / 2, tf.int32)
        offset_width = tf.cast((width - SIZE) / 2, tf.int32)
        cropped_image = tf.image.crop_to_bounding_box(
            image, offset_height, offset_width, SIZE, SIZE)

    return cropped_image


def _parse_train(item):
    image, label, text = _decode(item)
    rgb_image = _grayscale_to_rgb(image)
    resize_side = tf.random.uniform(
        [], minval=MIN_RESIZE, maxval=MAX_RESIZE + 1, dtype=tf.int32)
    resize_side = tf.cast(resize_side, tf.float32)
    resized_image = _resize(rgb_image, resize_side)
    cropped_image = _crop(resized_image, True)
    flipped_image = tf.image.random_flip_left_right(cropped_image)
    residual_image = flipped_image - [_R_MEAN, _G_MEAN, _B_MEAN]
    return residual_image, label, text


def _parse_eval(item):
    image, label, text = _decode(item)
    rgb_image = _grayscale_to_rgb(image)
    resized_image = _resize(rgb_image, MIN_RESIZE)
    cropped_image = _crop(resized_image)
    residual_image = cropped_image - [_R_MEAN, _G_MEAN, _B_MEAN]
    return residual_image, label, text


class TFRecordDataset(): # pylint: disable=R0902, R0903
    """parser TFRecord dataset and preprocessing"""
    def __init__( # pylint: disable=R0913
            self, path: str, is_training=False, keywords=None,
            num_parallel_reads=4, is_shuffle=False, buffer_size=1000,
            is_repeat=False, repeat_count=0, batch_size=32):

        self.filenames = os.listdir(path)
        self.path = []
        for i in self.filenames:
            if keywords is None or keywords in i:
                self.path.append(os.path.join(path, i))

        self.num_parallel_reads = num_parallel_reads
        self.dataset = tf.data.TFRecordDataset(
            self.path, num_parallel_reads=self.num_parallel_reads)

        self.is_training = is_training
        if self.is_training:
            self.dataset = self.dataset.map(_parse_train)
        else:
            self.dataset = self.dataset.map(_parse_eval)

        self.is_shuffle = is_shuffle
        self.buffer_size = buffer_size
        if self.is_shuffle:
            self.dataset = self.dataset.shuffle(self.buffer_size)

        self.is_repeat = is_repeat
        self.repeat_count = repeat_count
        if self.is_repeat:
            if self.repeat_count == 0:
                self.dataset = self.dataset.repeat()
            else:
                self.dataset = self.dataset.repeat(self.repeat_count)

        self.batch_size = batch_size
        self.dataset = self.dataset.batch(self.batch_size)

        self.iterator = tf.compat.v1.data.make_one_shot_iterator(self.dataset)
        self.images, self.labels, self.texts = self.iterator.get_next()

    def preview(self, columns=4):
        """Visual image data."""
        with tf.compat.v1.Session() as sess:
            images, labels, texts = sess.run(
                [self.images, self.labels, self.texts])

        means = np.array([_R_MEAN, _G_MEAN, _B_MEAN])
        plt.figure(figsize=(12, 3 * self.batch_size / columns))
        for i in range(self.batch_size):
            plt.subplot(self.batch_size / columns, columns, i + 1)
            plt.imshow((images[i] + means).astype(np.uint8))
            plt.axis('off')
            text = bytes.decode(texts[i]).split(',')[0]
            plt.title(str(labels[i]) + ': ' + text)


PATH, _ = os.path.split(os.path.realpath(__file__))
TMP = os.path.join(PATH, 'tmp')
RESULTS = os.path.join(PATH, 'results/retrain')
TRAIN_SIZE = 1281167
EVAL_SIZE = 50000
CATEGORY = 1000
INPUTS = 'resnet_v1_50/inputs'
LOGITS = 'resnet_v1_50/predictions/Reshape'
PREDICTIONS = 'resnet_v1_50/predictions/Reshape_1'


def parse_args():
    """Parse input arguments."""
    parser = argparse.ArgumentParser(description='ResNet-50 Retrain DEMO')

    parser.add_argument(
        '--config_defination', dest='config_defination', default=None, type=str,
        help='The simple configure define file.')
    parser.add_argument(
        '--batch_num', dest='batch_num', default=2, type=int,
        help='The Number of batches required by the inference quantization model.')
    parser.add_argument(
        '--train_set', dest='train_set', default=None, type=str,
        help='The path of ILSVRC-2012-CLS image classification dataset with "TFRecord" format for training.')
    parser.add_argument(
        '--train_keyword', dest='train_keyword', default=None, type=str,
        help='One keyword to filter training dataset filenames.')
    parser.add_argument(
        '--eval_set', dest='eval_set', default=None, type=str,
        help='The path of ILSVRC-2012-CLS image classification dataset with "TFRecord" format for evaluation.')
    parser.add_argument(
        '--eval_keyword', dest='eval_keyword', default=None, type=str,
        help='One keyword to filter evaluation dataset filenames.')
    parser.add_argument(
        '--num_parallel_reads', dest='num_parallel_reads', default=4, type=int,
        help='The number of files to read in parallel.')
    parser.add_argument(
        '--buffer_size', dest='buffer_size', default=1000, type=int,
        help='The number of elements from this dataset for shuffle. Invalid when "is_training" is False.')
    parser.add_argument(
        '--repeat_count', dest='repeat_count', default=0, type=int,
        help='The number of dataset repetitions, 0 means infinite loop. Invalid when "is_training" is False.')
    parser.add_argument(
        '--batch_size', dest='batch_size', default=32, type=int,
        help='The number of samples in each batch.')
    parser.add_argument(
        '--train_model', dest='train_model', type=str,
        default='./pre_model/resnet_v1_50_train.meta',
        help='The path of file containing a "MetaGraphDef" of ResNet V1 50 for training.')
    parser.add_argument(
        '--eval_model', dest='eval_model', type=str,
        default='./pre_model/resnet_v1_50_eval.meta',
        help='The path of file containing a "MetaGraphDef" of ResNet V1 50 for evaluation.')
    parser.add_argument(
        '--ckpt', dest='ckpt_path', type=str,
        default='./pre_model/resnet_v1_50',
        help='The path of ResNet V1 50 checkpoint.')
    parser.add_argument(
        '--learning_rate', dest='learning_rate', default=1e-6, type=float,
        help='The retrain learning rate.')
    parser.add_argument(
        '--momentum', dest='momentum', default=0.9, type=float,
        help='The momentum value of "RMSPropOptimizer".')
    parser.add_argument(
        '--save_interval', dest='save_interval', default=1000, type=int,
        help='The number of steps between checkpoints.')
    parser.add_argument(
        '--train_iter', dest='train_iter', default=1000, type=int,
        help='The number of retraining iterations.')

    return parser.parse_args()


def args_check(args):
    """Check the arguments."""
    if args.train_set is None:
        raise RuntimeError('Must specify a training dataset path!')
    args.train_set = os.path.realpath(args.train_set)
    if not os.access(args.train_set, os.F_OK):
        raise RuntimeError('Must specify a valid training dataset path!')

    if args.eval_set is None:
        raise RuntimeError('Must specify a evaluation dataset path!')
    args.eval_set = os.path.realpath(args.eval_set)
    if not os.access(args.eval_set, os.F_OK):
        raise RuntimeError('Must specify a valid evaluation dataset path!')

    if args.train_model is None:
        raise RuntimeError('Must specify a training model path!')
    args.train_model = os.path.realpath(args.train_model)
    if not os.access(args.train_model, os.F_OK):
        raise RuntimeError('Must specify a valid training model path!')

    if args.eval_model is None:
        raise RuntimeError('Must specify a evaluation model path!')
    args.eval_model = os.path.realpath(args.eval_model)
    if not os.access(args.eval_model, os.F_OK):
        raise RuntimeError('Must specify a valid evaluation model path!')

    if args.train_set == args.eval_set and (
            args.train_keyword is None and args.eval_keyword is None):
        LOGGER.push_warning_message(
            'Note that the training dataset has the same path as the '
            'evaluation dataset, it is recommended to use keyword '
            'arguments("--train_keyword" and "--eval_keyword") to distinguish '
            'between different dataset. Otherwise, the inference accuracy may '
            'be abnormal.', module_name='sample')


def mkdir(name):
    """Verify the path and create it."""
    if not os.access(name, os.F_OK):
        os.makedirs(name)


def evaluate(session, dataset):
    """Evaluate the model."""
    inputs = session.graph.get_tensor_by_name(INPUTS + ':0')
    predictions = session.graph.get_tensor_by_name(PREDICTIONS + ':0')

    count_1 = 0
    count_5 = 0
    for _ in trange(math.ceil(EVAL_SIZE / ARGS.batch_size), ncols=100):
        images, labels = session.run([dataset.images, dataset.labels])
        prediction = session.run(predictions, feed_dict={inputs: images})
        top_1 = np.argmax(prediction, axis=1) + 1
        top_5 = np.argpartition(prediction, -5, axis=1, )[:, -5:] + 1
        count_1 += np.sum(labels == top_1)
        count_5 += np.sum(
            labels.repeat(5).reshape([labels.shape[0], 5]) == top_5)
    acc_1 = count_1 / EVAL_SIZE * 100
    acc_5 = count_5 / EVAL_SIZE * 100
    return acc_1, acc_5


def evaluate_for_search_n(session, dataset, predictions_name, batch_num):
    """Evaluate the search N layers."""
    inputs = session.graph.get_tensor_by_name(INPUTS + ':0')
    predictions = session.graph.get_tensor_by_name(predictions_name + ':0')

    for _ in trange(math.ceil(batch_num), ncols=100):
        images = session.run(dataset.images)
        session.run(predictions, feed_dict={inputs: images})


def main(): # pylint: disable=R0914, R0915
    """main process"""
    args_check(ARGS)
    mkdir(TMP)
    mkdir(RESULTS)

    # Phase origin model accuracy
    # Step 1: Generate validation dataset.
    graph = tf.compat.v1.get_default_graph()

    dataset = TFRecordDataset(
        ARGS.eval_set,
        keywords=ARGS.eval_keyword,
        num_parallel_reads=ARGS.num_parallel_reads,
        batch_size=ARGS.batch_size)

    # Step 2: Load the evaluation model and validation it.
    session = tf.compat.v1.Session()
    saver = tf.compat.v1.train.import_meta_graph(ARGS.eval_model)
    saver.restore(session, ARGS.ckpt_path)
    acc_1, acc_5 = evaluate(session, dataset)
    session.close()
    print('The origin model top 1 accuracy = {}%.'.format(acc_1))
    print('The origin model top 5 accuracy = {}%.'.format(acc_5))

    # Phase retrain the model
    # Step 1: Generate training dataset.
    tf.compat.v1.reset_default_graph()
    graph = tf.compat.v1.get_default_graph()

    dataset = TFRecordDataset(
        ARGS.train_set,
        is_training=True,
        keywords=ARGS.train_keyword,
        num_parallel_reads=ARGS.num_parallel_reads,
        is_shuffle=True,
        is_repeat=True,
        batch_size=ARGS.batch_size)

    # Step 2: Load the training model.
    saver = tf.compat.v1.train.import_meta_graph(ARGS.train_model)
    input_1 = graph.get_tensor_by_name(INPUTS + ':0')
    logits = graph.get_tensor_by_name(LOGITS + ':0')

    # Step 3: Create the retraining configuration file.
    config_file = os.path.join(TMP, 'config.json')
    record_file = os.path.join(TMP, 'record.txt')
    config_defination = ARGS.config_defination
    amct.create_quant_retrain_config(config_file, graph, config_defination)

    # Step 4: Generate the retraining model in default graph and create the
    # quantization factor record_file.
    retrain_ops = amct.create_quant_retrain_model(graph, config_file,
                                                  record_file)

    # Step 5: Set the variables which needed to save.
    variables_to_save = tf.compat.v1.global_variables()
    saver_save = tf.compat.v1.train.Saver(variables_to_save)

    # Step 6: Calculate loss function and create an optimizer.
    labels = tf.one_hot(dataset.labels - 1, CATEGORY)
    input_2 = tf.compat.v1.placeholder(
        tf.float32, shape=[ARGS.batch_size, CATEGORY])


    l2_variables = []
    for i in tf.compat.v1.trainable_variables():
        if 'BatchNorm' not in i.name and 'ULQ' not in i.name:
            l2_variables.append(tf.nn.l2_loss(tf.cast(i, tf.float32)))
    l2_loss = 1e-4 * tf.add_n(l2_variables)
    cross_entropy = tf.nn.softmax_cross_entropy_with_logits_v2(
        labels=input_2, logits=logits) / ARGS.batch_size
    loss = cross_entropy + l2_loss

    optimizer = tf.compat.v1.train.RMSPropOptimizer(
        ARGS.learning_rate, momentum=ARGS.momentum)
    train_op = optimizer.minimize(loss)

    # Step 7: Set a summary of loss for tensorboard.
    tf.compat.v1.summary.scalar('l2_loss', l2_loss)
    tf.compat.v1.summary.scalar('cross_entropy', tf.reduce_sum(cross_entropy))
    tf.compat.v1.summary.scalar('loss', tf.reduce_sum(cross_entropy) + l2_loss)
    write_op = tf.compat.v1.summary.merge_all()
    summary_writer = tf.compat.v1.summary.FileWriter(TMP)

    # Step 8: Restore the checkpoint and retrain the model. Also, save
    # checkpoint every 'save_interval' steps.
    session = tf.compat.v1.Session()
    session.run(tf.compat.v1.global_variables_initializer())
    saver.restore(session, ARGS.ckpt_path)
    retrain_ckpt = os.path.join(TMP, 'resnet_v1_50_retrain')
    for i in trange(ARGS.train_iter, ncols=100):
        image, label = session.run([dataset.images, labels])
        summary_loss, _ = session.run(
            [write_op, train_op], feed_dict={input_1: image, input_2: label})
        summary_writer.add_summary(summary_loss, i)
        if i % ARGS.save_interval == 0:
            saver_save.save(session, retrain_ckpt, global_step=i)

    saver_save.save(session, retrain_ckpt, global_step=ARGS.train_iter)
    session.close()

    # Phase convert retrain model
    # Step 1: Load the evaluation model.
    tf.compat.v1.reset_default_graph()
    graph = tf.compat.v1.get_default_graph()

    saver = tf.compat.v1.train.import_meta_graph(ARGS.eval_model)

    # Step 2: Generate the retraining model accroding to the retraining
    # configuration file which created last phase.
    retrain_ops = amct.create_quant_retrain_model(graph, config_file,
                                                  record_file)

    # Step 3: Set the variables which needed to restore.
    variables_to_restore = tf.compat.v1.global_variables()
    saver_restore = tf.compat.v1.train.Saver(variables_to_restore)

    # Step 4: Restore the variables and use the eval_set inference output node
    # (retrain_ops[-1]) to write the quantization factor into the record_file.
    session = tf.compat.v1.Session()
    session.run(tf.compat.v1.global_variables_initializer())
    retrain_ckpt = retrain_ckpt + '-' + str(ARGS.train_iter)
    saver_restore.restore(session, retrain_ckpt)
    dataset = TFRecordDataset(
        ARGS.eval_set,
        keywords=ARGS.eval_keyword,
        num_parallel_reads=ARGS.num_parallel_reads,
        batch_size=ARGS.batch_size)
    evaluate_for_search_n(session, dataset, retrain_ops[-1].name[:-2],
                          ARGS.batch_num)

    # Step 5: Convert all variables to constants and finally save as 'pb' file.
    constant_graph = tf.compat.v1.graph_util.convert_variables_to_constants(
        session, graph.as_graph_def(), [PREDICTIONS])
    pb_path = os.path.join(TMP, 'resnet_v1_50.pb')
    with tf.io.gfile.GFile(pb_path, 'wb') as pb_file:
        pb_file.write(constant_graph.SerializeToString())
    session.close()

    # Step 6: Convert origin 'pb' model file to fake quantized 'pb'
    # model, using the quantization factor record_file.
    quantized_pb_path = os.path.join(RESULTS, 'resnet_v1_50')
    amct.save_quant_retrain_model(
        pb_path, [PREDICTIONS], record_file, quantized_pb_path)

    # Phase verification
    # Step 1: Generate validation dataset.
    tf.compat.v1.reset_default_graph()
    graph = tf.compat.v1.get_default_graph()

    dataset = TFRecordDataset(
        ARGS.eval_set,
        keywords=ARGS.eval_keyword,
        num_parallel_reads=ARGS.num_parallel_reads,
        batch_size=ARGS.batch_size)

    # Step 2: Load the fake quantized model and validation it.
    quantized_pb_file = quantized_pb_path + '_quantized.pb'
    with tf.io.gfile.GFile(quantized_pb_file, 'rb') as fid:
        graph_def = tf.compat.v1.GraphDef()
        graph_def.ParseFromString(fid.read())
    tf.import_graph_def(graph_def, name='')

    session = tf.compat.v1.Session()
    acc_1, acc_5 = evaluate(session, dataset)
    session.close()
    print('The model after retraining top 1 accuracy = {}%.'.format(acc_1))
    print('The model after retraining top 5 accuracy = {}%.'.format(acc_5))


if __name__ == '__main__':
    ARGS = parse_args()
    main()
